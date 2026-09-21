#pragma once
#include "NvseApi.h"
#include <cstring>

namespace nvvr {
enum class Result { Ready, MissingInterface, Editor, OldNvse, WrongRuntime,
    NoGore, MissingFunctions, MissingMessaging, OldMessaging, MissingListener,
    InvalidHandle, RegistrationFailed };
inline const char* Name(Result r) {
    constexpr const char* names[] = {"ready", "missing interface", "editor",
        "xNVSE older than 6.4.8", "unsupported runtime", "NoGore",
        "missing interface functions", "missing messaging", "old messaging",
        "missing listener function", "invalid plugin handle", "registration failed"};
    return names[static_cast<unsigned>(r)];
}
inline Result Validate(const Interface* api) {
    if (!api) return Result::MissingInterface;
    if (api->isEditor) return Result::Editor;
    if (api->nvseVersion < MinimumNvse) return Result::OldNvse;
    if (api->runtimeVersion != Runtime) return Result::WrongRuntime;
    if (api->isNogore) return Result::NoGore;
    if (!api->QueryInterface || !api->GetPluginHandle || !api->GetRuntimeDirectory)
        return Result::MissingFunctions;
    return Result::Ready;
}
inline Result Attach(const Interface* api, Callback callback) {
    const auto valid = Validate(api);
    if (valid != Result::Ready) return valid;
    auto* messages = static_cast<Messaging*>(api->QueryInterface(MessagingId));
    if (!messages) return Result::MissingMessaging;
    if (messages->version < 4) return Result::OldMessaging;
    if (!messages->RegisterListener) return Result::MissingListener;
    const auto handle = api->GetPluginHandle();
    if (handle == 0xFFFFFFFFu) return Result::InvalidHandle;
    if (!messages->RegisterListener(handle, "NVSE", callback)) return Result::RegistrationFailed;
    return Result::Ready;
}
// Counts only; never dereferences engine/event payload pointers.
struct Events {
    std::uint64_t loops = 0, presents = 0;
    bool Observe(const Message* message) {
        if (!message || !message->sender || std::strcmp(message->sender, "NVSE") != 0)
            return false;
        if (message->type == MainLoop) return Sample(++loops);
        if (message->type == FramePresent) return Sample(++presents);
        switch (message->type) {
        case 0: case 1: case 2: case 3: case 4: case 6: case 7:
        case 8: case 9: case 14: case 18: case 23: return true;
        default: return false;
        }
    }
    static bool Sample(std::uint64_t count) { return count == 1 || count % 600 == 0; }
};
}
