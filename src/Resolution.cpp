#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "Resolution.h"
#include "UiSizeLogic.h"
#include "ProcessReader.h"
#include "vr/OpenVRBackend.h"
#include <cstdio>
namespace nvvr::resolution {
namespace {
constexpr probe::Address Site=0x004DAA4C,Original=0x00E76210;
using FactoryFn=std::uintptr_t(__cdecl*)(Arguments);
FactoryFn factory=reinterpret_cast<FactoryFn>(Original);
hook::Logger logger=nullptr;
#ifdef NVVR_RESOLUTION_TESTS
Size testWanted{};
#endif
Size Recommended(){
#ifdef NVVR_RESOLUTION_TESTS
    return testWanted;
#else
    obvr::vr::OpenVRBackend query;
    Size recommended{};
    const bool available=query.Start(false) && query.GetRecommendedRenderTargetSize(recommended.width,recommended.height);
    query.Stop();
    const auto wanted=Choose(available,recommended);
    char text[240];sprintf_s(text,"resolution recommendation=%ux%u selected=%ux%u available=%u max_edge=4096",recommended.width,recommended.height,wanted.width,wanted.height,unsigned(available));if(logger)logger(text);
    return wanted;
#endif
}
std::uintptr_t __cdecl Create(Arguments args){
    __declspec(align(16)) unsigned char state[512];
    __asm fxsave state
    layout={args.words[0],args.words[1]};
    const auto wanted=Recommended();
    // The game's factory receives its original floating-point environment,
    // not whatever the temporary OpenVR client left behind.
    __asm fxrstor state
        ui::sourceSize={};
    const auto result=Make(args,wanted,[](Arguments attempt){
        const auto created=factory(attempt);
        if(created)ui::sourceSize=ui::SourceSize({attempt.words[0],attempt.words[1]},layout);
        return created;
    });
    // Integer-only UI sizing leaves the factory's floating-point result untouched.
    return result;
}
bool Protect(void*,probe::Address address,std::uint32_t protection,std::uint32_t* previous){DWORD old=0;const bool ok=VirtualProtect(reinterpret_cast<void*>(address),5,protection,&old)!=0;*previous=old;return ok;}
void Write(void*,probe::Address address,const void* bytes,std::size_t size){std::memcpy(reinterpret_cast<void*>(address),bytes,size);}
bool Flush(void*,probe::Address address){return FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),5)!=0;}
}
void Install(hook::Logger log){
    logger=log;
    const hook::PatchOps ops{nullptr,probe::CopyCurrentProcess,Protect,Write,Flush};
    const auto result=hook::Install(ops,Site,static_cast<probe::Address>(reinterpret_cast<std::uintptr_t>(&Create)),Original);
    char text[180];sprintf_s(text,"resolution_hook installed=%u mismatch=%u restore_failed=%u",unsigned(result.installed),unsigned(result.mismatch),unsigned(result.restoreFailed));log(text);
}
#ifdef NVVR_RESOLUTION_TESTS
std::uintptr_t TestCreate(Arguments args,Size wanted,CreateFn original){factory=original;testWanted=wanted;return Create(args);}
#endif
}
