#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>

namespace nvvr::probe {
using Address = std::uint32_t;
// Read-only profile: docs/engine-probe.md contains source and binary evidence.
inline constexpr Address RendererGlobal = 0x011F4748;
inline constexpr Address RendererAlias = 0x011C73B4;
inline constexpr Address WorldGlobal = 0x011DEB7C;
inline constexpr Address RendererVtable = 0x010EE4BC;
inline constexpr Address SceneVtable = 0x01083B5C;
inline constexpr Address CameraVtable = 0x0109CB9C;
struct Reader {
    void* context;
    bool (*copy)(void*, Address, void*, std::size_t);
    bool Read(Address base, std::size_t offset, void* out, std::size_t size) const {
        const auto start = static_cast<std::uint64_t>(base) + offset;
        if (!copy || !out || size == 0 || base < 0x10000 ||
            start > UINT32_MAX || size > UINT32_MAX - start + 1) return false;
        return copy(context, static_cast<Address>(start), out, size);
    }
    template<class T> bool Value(Address base, std::size_t offset, T& out) const {
        return Read(base, offset, &out, sizeof(out));
    }
};
struct Signature { Address address; std::uint8_t bytes[6]; std::size_t length; };
inline constexpr Signature Signatures[] = {
    {0x0045C673, {0xB9,0x7C,0xEB,0x1D,0x01},5}, // world getter
    {0x006629FA, {0x81,0xC1,0xAC,0,0,0},6},     // scene->camera
    {0x00A71308, {0xC7,0x06,0x9C,0xCB,0x09,0x01},6}, // camera ctor vtable
    {0x00A62B42, {0x89,0x35,0x48,0x47,0x1F,0x01},6}, // renderer singleton store
    {0x00E6C243, {0x8B,0x86,0x88,0x02,0,0},6}, // renderer->device
};
inline bool VerifyProfile(const Reader& reader, Address moduleBase) {
    if (moduleBase != 0x00400000) return false;
    for (const auto& sig : Signatures) {
        std::uint8_t actual[6]{};
        if (!reader.Read(sig.address, 0, actual, sig.length) ||
            std::memcmp(actual, sig.bytes, sig.length) != 0) return false;
    }
    return true;
}
enum class State { Absent, Unreadable, WrongType, InvalidValues, Changed, Ready };
inline const char* Name(State state) {
    constexpr const char* names[] = {"absent", "unreadable", "wrong-type", "invalid-values", "changed-during-read", "ready"};
    return names[static_cast<unsigned>(state)];
}
struct Transform { float rotation[9]; float position[3]; float scale; };
struct Frustum { float left, right, top, bottom, nearPlane, farPlane; std::uint8_t ortho, pad[3]; };
struct Camera {
    Address vtable, refCount, name;
    std::uint8_t pad0C[0x18-0x0C];
    Address parent;
    std::uint8_t pad1C[0x34-0x1C];
    Transform local, world;
    float worldToCamera[16];
    Frustum frustum;
    float minNear, maxRatio, viewport[4], lodAdjust;
};
static_assert(sizeof(Transform) == 0x34);
static_assert(offsetof(Camera, local) == 0x34 && offsetof(Camera, world) == 0x68);
static_assert(offsetof(Camera, frustum) == 0xDC && sizeof(Camera) == 0x114);

inline bool PlausibleTransform(const Transform& transform) {
    for (float x : transform.rotation) if (!std::isfinite(x)) return false;
    for (float x : transform.position) if (!std::isfinite(x)) return false;
    if (!std::isfinite(transform.scale) || transform.scale <= 0) return false;
    // Check the basis independently of the camera coordinate convention.
    for (unsigned row = 0; row < 3; ++row) {
        float length = 0;
        for (unsigned col = 0; col < 3; ++col) length += transform.rotation[row*3+col] * transform.rotation[row*3+col];
        if (std::fabs(length - 1) > 0.02f) return false;
        for (unsigned other = 0; other < row; ++other) {
            float dot = 0;
            for (unsigned col = 0; col < 3; ++col) dot += transform.rotation[row*3+col] * transform.rotation[other*3+col];
            if (std::fabs(dot) > 0.02f) return false;
        }
    }
    return true;
}
inline bool PlausibleFrustum(const Frustum& f) {
    const float values[] = {f.left,f.right,f.top,f.bottom,f.nearPlane,f.farPlane};
    for (float x : values) if (!std::isfinite(x)) return false;
    return f.left < f.right && f.bottom < f.top && f.nearPlane > 0 &&
        f.farPlane > f.nearPlane && f.ortho == 0;
}
struct RendererSample {
    State state = State::Unreadable;
    Address renderer = 0, alias = 0, device = 0, deviceVtable = 0;
    bool aliasReadable = false;
    std::uint32_t width = 0, height = 0;
};
inline RendererSample ReadRenderer(const Reader& reader) {
    RendererSample s;
    s.aliasReadable = reader.Value(RendererAlias, 0, s.alias);
    if (!reader.Value(RendererGlobal, 0, s.renderer)) return s;
    if (!s.renderer) { s.state = State::Absent; return s; }
    Address vtable = 0;
    if (!reader.Value(s.renderer, 0, vtable)) return s;
    if (vtable != RendererVtable) { s.state = State::WrongType; return s; }
    if (!reader.Value(s.renderer, 0x288, s.device) ||
        !reader.Value(s.renderer, 0xA98, s.width) || !reader.Value(s.renderer, 0xA9C, s.height)) return s;
    if (!s.device) { s.state = State::Absent; return s; }
    if (!reader.Value(s.device, 0, s.deviceVtable)) return s;
    Address firstMethod = 0;
    if (!reader.Value(s.deviceVtable, 0, firstMethod)) return s;
    if (!firstMethod || s.width == 0 || s.height == 0 || s.width > 32768 || s.height > 32768) {
        s.state = State::InvalidValues; return s;
    }
    Address again = 0, deviceAgain = 0;
    if (!reader.Value(RendererGlobal, 0, again) || !reader.Value(s.renderer, 0x288, deviceAgain)) return s;
    s.state = (again == s.renderer && deviceAgain == s.device) ? State::Ready : State::Changed;
    return s;
}
struct CameraSample {
    State state = State::Unreadable;
    Address scene = 0, camera = 0;
    Camera data{};
};
inline CameraSample ReadCamera(const Reader& reader) {
    CameraSample s;
    if (!reader.Value(WorldGlobal, 0, s.scene)) return s;
    if (!s.scene) { s.state = State::Absent; return s; }
    Address vtable = 0;
    if (!reader.Value(s.scene, 0, vtable)) return s;
    if (vtable != SceneVtable) { s.state = State::WrongType; return s; }
    if (!reader.Value(s.scene, 0xAC, s.camera)) return s;
    if (!s.camera) { s.state = State::Absent; return s; }
    if (!reader.Value(s.camera, 0, s.data)) return s;
    if (s.data.vtable != CameraVtable) { s.state = State::WrongType; return s; }
    if (!PlausibleTransform(s.data.local) || !PlausibleTransform(s.data.world) || !PlausibleFrustum(s.data.frustum)) {
        s.state = State::InvalidValues; return s;
    }
    Address sceneAgain = 0, cameraAgain = 0;
    if (!reader.Value(WorldGlobal, 0, sceneAgain) || !reader.Value(s.scene, 0xAC, cameraAgain)) return s;
    s.state = (sceneAgain == s.scene && cameraAgain == s.camera) ? State::Ready : State::Changed;
    return s;
}
}
