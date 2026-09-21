#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Diagnostics.h"
#include "ProcessReader.h"
#include "CameraHooks.h"
#include "Rotation.h"
#include "Headset.h"
#include "Stereo.h"
#include "Ui.h"
#include "Aim.h"
#include "SessionLogic.h"
#include "Resolution.h"
#include <cstdio>
#include <mutex>
#include <string>

namespace {
std::mutex guard;
std::string logPath;
nvvr::Events events;
bool loaded = false;
bool probeEnabled = false;
const nvvr::probe::Reader processReader{nullptr, nvvr::probe::CopyCurrentProcess};

bool Log(const char* text) {
    FILE* file = nullptr;
    if (fopen_s(&file, logPath.c_str(), "a") != 0) return false;
    const bool written = std::fprintf(file, "[%lu] %s\n", GetTickCount(), text) >= 0;
    const bool closed = std::fclose(file) == 0;
    return written && closed;
}
void OnMessage(nvvr::Message* message) {
    std::lock_guard<std::mutex> lock(guard);
    if(message && message->sender && std::strcmp(message->sender,"NVSE")==0){
        if(probeEnabled && message->type==nvvr::MainLoop){nvvr::ui::EnsureDevice(Log);if(!nvvr::ui::HasPresentHook())nvvr::rotation::Poll(Log);}
        const auto action=nvvr::OnLifecycle(message->type);
        if(action==nvvr::SessionAction::Stop){nvvr::rotation::Reset(Log);nvvr::stereo::Reset();}
        else if(action==nvvr::SessionAction::ResetWorld){nvvr::stereo::Reset();nvvr::vr::ResetHud();}
    }
    if (message && message->sender && std::strcmp(message->sender,"NVSE")==0 && message->type==nvvr::FramePresent){nvvr::hook::Present();if(probeEnabled){nvvr::ui::EnsureDevice(Log);if(!nvvr::ui::HasPresentHook())nvvr::ui::Present();}}
    if (!events.Observe(message)) return;
    char text[256];
    sprintf_s(text, "event=%u payload_bytes=%u thread=%lu loops=%llu presents=%llu",
        message->type, message->dataLen, GetCurrentThreadId(),
        static_cast<unsigned long long>(events.loops),
        static_cast<unsigned long long>(events.presents));
    Log(text);
    if (probeEnabled && (message->type==nvvr::FramePresent || message->type==1 || message->type==7)){nvvr::hook::Report(Log);nvvr::aim::Report(Log);nvvr::ui::Report(Log);}
    if (!probeEnabled || message->type != nvvr::FramePresent) return;
    const auto renderer = nvvr::probe::ReadRenderer(processReader);
    char probeText[512];
    sprintf_s(probeText, "renderer=%s ptr=%08X alias=%08X alias_read=%u device=%08X device_vtable=%08X size=%ux%u",
        nvvr::probe::Name(renderer.state), renderer.renderer, renderer.alias, unsigned(renderer.aliasReadable),
        renderer.device, renderer.deviceVtable, renderer.width, renderer.height);
    Log(probeText);
    const auto camera = nvvr::probe::ReadCamera(processReader);
    sprintf_s(probeText, "camera=%s scene=%08X ptr=%08X parent=%08X local=(%.4f,%.4f,%.4f) world=(%.4f,%.4f,%.4f) frustum=(%.4f,%.4f,%.4f,%.4f;%.4f,%.4f) ortho=%u",
        nvvr::probe::Name(camera.state), camera.scene, camera.camera, camera.data.parent,
        camera.data.local.position[0],camera.data.local.position[1],camera.data.local.position[2],
        camera.data.world.position[0],camera.data.world.position[1],camera.data.world.position[2],
        camera.data.frustum.left,camera.data.frustum.right,camera.data.frustum.top,camera.data.frustum.bottom,
        camera.data.frustum.nearPlane,camera.data.frustum.farPlane,unsigned(camera.data.frustum.ortho));
    Log(probeText);
    sprintf_s(probeText, "camera_world_basis=(%.5f,%.5f,%.5f;%.5f,%.5f,%.5f;%.5f,%.5f,%.5f) scale=%.5f",
        camera.data.world.rotation[0],camera.data.world.rotation[1],camera.data.world.rotation[2],
        camera.data.world.rotation[3],camera.data.world.rotation[4],camera.data.world.rotation[5],
        camera.data.world.rotation[6],camera.data.world.rotation[7],camera.data.world.rotation[8],camera.data.world.scale);
    Log(probeText);
}
}

extern "C" bool NVSEPlugin_Query(const nvvr::Interface* api, nvvr::Info* info) {
    std::lock_guard<std::mutex> lock(guard);
    if (!info) return false;
    *info = {1, "NVVR", 14};
    const auto result = nvvr::Validate(api);
    // Refused hosts receive false; their loader records the refusal. Never call
    // functions on an unsupported ABI just to produce our own log.
    if (result != nvvr::Result::Ready) return false;
    const char* directory = api->GetRuntimeDirectory();
    if (!directory || !*directory) return false;
    logPath = directory;
    logPath += "/NVVR-diagnostics.log";
    nvvr::vr::Configure(Log);
    return Log("NVVR 0.0.14: integrated seated VR test build; VR automatic, Delete recenter; stereo, head aiming, spatial UI");
}

extern "C" bool NVSEPlugin_Load(const nvvr::Interface* api) {
    // RegisterListener is external code: do not hold the callback's mutex here.
    if (logPath.empty()) return false;
    if (nvvr::Validate(api) != nvvr::Result::Ready) return false;
    if (loaded) return true;
    probeEnabled = nvvr::probe::VerifyProfile(processReader,
        static_cast<nvvr::probe::Address>(reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))));
    Log(probeEnabled ? "Engine probe enabled: all five binary signatures match" : "Engine probe disabled: executable base/signatures do not match");
    if(probeEnabled)nvvr::vr::PreloadClient();
    if (!Log("Load: registering NVSE diagnostics listener")) return false;
    const auto result = nvvr::Attach(api, OnMessage);
    Log(nvvr::Name(result));
    loaded = result == nvvr::Result::Ready;
    // Once CALLs reference this DLL, keep it loaded even if patch setup reports
    // a partial failure. Never install before listener registration succeeds.
    if (loaded && probeEnabled) nvvr::resolution::Install(Log);
    if (loaded && probeEnabled) nvvr::hook::InstallHooks(Log);
    if (loaded && probeEnabled) nvvr::stereo::Install(Log);
    if (loaded && probeEnabled){nvvr::aim::Install(Log);nvvr::ui::Install(Log);}
    return loaded;
}
