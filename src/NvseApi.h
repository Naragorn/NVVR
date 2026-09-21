#pragma once
#include <cstdint>
#include <cstddef>

// Minimal ABI prefix, not an engine layout. Evidence: docs/SOURCES.md.
namespace nvvr {
using U32 = std::uint32_t;
struct CommandInfo;
struct Interface {
    U32 nvseVersion, runtimeVersion, editorVersion, isEditor;
    bool (*RegisterCommand)(CommandInfo*);
    void (*SetOpcodeBase)(U32);
    void* (*QueryInterface)(U32);
    U32 (*GetPluginHandle)();
    bool (*RegisterTypedCommand)(CommandInfo*, U32);
    const char* (*GetRuntimeDirectory)();
    U32 isNogore;
};
struct Info { U32 infoVersion; const char* name; U32 version; };
struct Message { const char* sender; U32 type, dataLen; void* data; };
using Callback = void (*)(Message*);
struct Messaging {
    U32 version;
    bool (*RegisterListener)(U32, const char*, Callback);
    bool (*Dispatch)(U32, U32, void*, U32, const char*);
};
inline constexpr U32 Runtime = 0x040020D0;
// Deliberately conservative floor: the source inspected for this prototype.
inline constexpr U32 MinimumNvse = 0x06040080; // MAKE_NEW_VEGAS_VERSION(6,4,8)
inline constexpr U32 MessagingId = 2;
inline constexpr U32 MainLoop = 20;
inline constexpr U32 FramePresent = 24;
static_assert(sizeof(void*) == 4);
static_assert(offsetof(Interface, QueryInterface) == 0x18);
static_assert(offsetof(Interface, GetPluginHandle) == 0x1C);
static_assert(offsetof(Interface, isNogore) == 0x28);
static_assert(sizeof(Info) == 12 && sizeof(Message) == 16);
static_assert(sizeof(Messaging) == 12);
}
