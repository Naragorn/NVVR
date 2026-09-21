#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Diagnostics.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

namespace {
int checks = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #expr); std::exit(1); } } while (0)
nvvr::Callback callback = nullptr;
bool registerOk = true;
unsigned registrations = 0;
nvvr::U32 handle = 1;
const char* directory = ".";
bool Register(nvvr::U32 h, const char* sender, nvvr::Callback cb) {
    CHECK(h == handle); CHECK(std::strcmp(sender, "NVSE") == 0);
    ++registrations;
    if (registerOk) {
        callback = cb;
        nvvr::Message immediate{"NVSE", 0, 0, nullptr};
        cb(&immediate); // Also checks that synchronous callbacks cannot deadlock.
    }
    return registerOk;
}
nvvr::Messaging messages{4, Register, nullptr};
bool missingMessages = false;
void* Query(nvvr::U32 id) { CHECK(id == 2); return missingMessages ? nullptr : &messages; }
nvvr::U32 Handle() { return handle; }
const char* Directory() { return directory; }
nvvr::Interface Good() {
    return {nvvr::MinimumNvse, nvvr::Runtime, 0, 0, nullptr, nullptr,
        Query, Handle, nullptr, Directory, 0};
}
void Ignore(nvvr::Message*) {}
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    using namespace nvvr;
    CHECK(Validate(nullptr) == Result::MissingInterface);
    CHECK(Attach(nullptr, Ignore) == Result::MissingInterface);
    auto api = Good();
    CHECK(Validate(&api) == Result::Ready);
    api.isEditor = 1; CHECK(Validate(&api) == Result::Editor); api = Good();
    --api.nvseVersion; CHECK(Validate(&api) == Result::OldNvse); api = Good();
    ++api.nvseVersion; CHECK(Validate(&api) == Result::Ready); api = Good();
    --api.runtimeVersion; CHECK(Validate(&api) == Result::WrongRuntime); api = Good();
    ++api.runtimeVersion; CHECK(Validate(&api) == Result::WrongRuntime); api = Good();
    api.isNogore = 1; CHECK(Validate(&api) == Result::NoGore); api = Good();
    // All meaningful combinations of the three required function slots.
    for (unsigned mask = 0; mask < 8; ++mask) {
        api = Good();
        if (mask & 1) api.QueryInterface = nullptr;
        if (mask & 2) api.GetPluginHandle = nullptr;
        if (mask & 4) api.GetRuntimeDirectory = nullptr;
        CHECK(Validate(&api) == (mask ? Result::MissingFunctions : Result::Ready));
    }
    api = Good();
    missingMessages = true; CHECK(Attach(&api, Ignore) == Result::MissingMessaging); missingMessages = false;
    messages.version = 3; CHECK(Attach(&api, Ignore) == Result::OldMessaging); messages.version = 4;
    messages.RegisterListener = nullptr; CHECK(Attach(&api, Ignore) == Result::MissingListener); messages.RegisterListener = Register;
    handle = 0xFFFFFFFF; CHECK(Attach(&api, Ignore) == Result::InvalidHandle); handle = 1;
    registerOk = false; CHECK(Attach(&api, Ignore) == Result::RegistrationFailed); registerOk = true;
    CHECK(Attach(&api, Ignore) == Result::Ready);
    for (unsigned i = 0; i <= static_cast<unsigned>(Result::RegistrationFailed); ++i)
        CHECK(std::strlen(Name(static_cast<Result>(i))) > 0);

    Events state;
    CHECK(!state.Observe(nullptr));
    Message m{nullptr, 0, 0, nullptr}; CHECK(!state.Observe(&m));
    m.sender = "OtherPlugin"; CHECK(!state.Observe(&m));
    m.sender = "NVSE"; CHECK(state.Observe(&m));
    for (auto type : {MainLoop, FramePresent}) {
        m.type = type;
        for (unsigned i = 1; i <= 1201; ++i)
            CHECK(state.Observe(&m) == (i == 1 || i % 600 == 0));
    }
    CHECK(state.loops == 1201 && state.presents == 1201);
    for (unsigned type = 0; type <= 32; ++type) {
        if (type == MainLoop || type == FramePresent) continue;
        m.type = type;
        const bool lifecycle = type == 0 || type == 1 || type == 2 || type == 3 || type == 4 ||
            type == 6 || type == 7 || type == 8 || type == 9 || type == 14 || type == 18 || type == 23;
        CHECK(state.Observe(&m) == lifecycle);
    }
    m.type = 9999; CHECK(!state.Observe(&m));

    // Exercise the actual built 32-bit DLL and its undecorated loader exports.
    const auto module = LoadLibraryA(argv[1]); CHECK(module != nullptr);
    using QueryFn = bool (*)(const Interface*, Info*);
    using LoadFn = bool (*)(const Interface*);
    const auto query = reinterpret_cast<QueryFn>(GetProcAddress(module, "NVSEPlugin_Query"));
    const auto load = reinterpret_cast<LoadFn>(GetProcAddress(module, "NVSEPlugin_Load"));
    CHECK(query && load);
    CHECK(!load(&api));
    Info info{};
    CHECK(!query(&api, nullptr)); CHECK(!query(nullptr, &info));
    directory = nullptr; CHECK(!query(&api, &info));
    directory = ""; CHECK(!query(&api, &info));
    CHECK(GetFileAttributesA("nvvr-tests-absent-directory") == INVALID_FILE_ATTRIBUTES);
    directory = "nvvr-tests-absent-directory/subdirectory"; CHECK(!query(&api, &info));
    CHECK(!load(&api)); // Logging cannot open the absent directory.
    directory = ".";
    CHECK(query(&api, &info));
    CHECK(info.infoVersion == 1 && info.version == 14 && std::strcmp(info.name, "NVVR") == 0);
    CHECK(!load(nullptr));
    registerOk = false; CHECK(!load(&api)); registerOk = true;
    CHECK(load(&api));
    const auto before = registrations;
    CHECK(load(&api)); CHECK(registrations == before);
    CHECK(callback != nullptr);
    callback(nullptr);
    m = {"NVSE", MainLoop, 0, nullptr};
    for (unsigned i = 0; i < 601; ++i) callback(&m);
    m.type = FramePresent; callback(&m);
    m.type = 8; m.data = reinterpret_cast<void*>(1); callback(&m); // Do not dereference scalar event data.
    std::ifstream log("NVVR-diagnostics.log");
    const std::string text((std::istreambuf_iterator<char>(log)), std::istreambuf_iterator<char>());
    CHECK(text.find("loops=600") != std::string::npos);
    CHECK(text.find("presents=1") != std::string::npos);
    CHECK(text.find("Engine probe disabled:") != std::string::npos);
    CHECK(FreeLibrary(module));
    std::printf("PASS: %d checks, core flow cases and actual x86 DLL host simulation\n", checks);
}
