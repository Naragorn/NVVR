#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Aim.h"
#include "AimLogic.h"
#include "Stereo.h"
#include "Rotation.h"
#include "ProcessReader.h"
#include <atomic>
#include <cstdio>
namespace nvvr::aim {
namespace {
constexpr probe::Address Site=0x005245BD,Original=0x009BCA60;
const probe::Reader reader{nullptr,probe::CopyCurrentProcess};
probe::Address originalAddress=Original;
#ifdef NVVR_AIM_TESTS
bool testing=false;
probe::Transform testGaze{};
probe::Address testPlayer=0;
#endif
bool ReadAim(probe::Address& player,probe::Transform& gaze){
#ifdef NVVR_AIM_TESTS
    if(testing){player=testPlayer;gaze=testGaze;return true;}
#endif
    return reader.Value(0x011DEA3C,0,player)&&stereo::Gaze(gaze);
}
std::atomic<std::uint64_t> applied{0},refused{0},failed{0},restoreFailed{0};
std::atomic<bool> fault{false};
void __cdecl Adjust(probe::Address stack){
    alignas(16) unsigned char fp[512];
    __asm fxsave fp
    Launch args{};probe::Address player=0;probe::Transform gaze{};
    if(!fault.load() && reader.Read(stack,4,&args,sizeof(args)) && ReadAim(player,gaze)){
        const auto r=Apply(true,player,args,gaze,stack+4+7*4,nullptr,rotation::WriteCurrent);
        if(r==Result::Applied)++applied;
        else if(r==Result::Refused)++refused;
        else {++failed;fault=true;if(r==Result::RestoreFailed)++restoreFailed;}
    }else ++refused;
    __asm fxrstor fp
}
// 005245BD passes sixteen cdecl DWORDs; the caller removes 40h bytes.
// Change only the two outgoing angle slots, then tail-call the original.
__declspec(naked) std::uint32_t __cdecl Entry(Launch){__asm {
    push eax
    lea eax,[esp+4]
    pushfd
    pushad
    push eax
    call Adjust
    add esp,4
    popad
    popfd
    pop eax
    push dword ptr originalAddress
    ret
}}
bool Protect(void*,probe::Address address,std::uint32_t protection,std::uint32_t* previous){DWORD old=0;const bool ok=VirtualProtect(reinterpret_cast<void*>(address),5,protection,&old)!=0;*previous=old;return ok;}
void Write(void*,probe::Address address,const void* bytes,std::size_t size){std::memcpy(reinterpret_cast<void*>(address),bytes,size);}
bool Flush(void*,probe::Address address){return FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),5)!=0;}
}
#ifdef NVVR_AIM_TESTS
std::uint32_t TestLaunch(Launch args,const probe::Transform& gaze,probe::Address player,std::uint32_t(__cdecl* target)(Launch)){
    testing=true;testGaze=gaze;testPlayer=player;
    originalAddress=static_cast<probe::Address>(reinterpret_cast<std::uintptr_t>(target));
    const auto result=Entry(args);
    originalAddress=Original;testing=false;return result;
}
#endif
void Install(hook::Logger log){
    std::array<unsigned char,3> cleanup{};
    if(!reader.Read(Site,5,cleanup.data(),cleanup.size())||cleanup!=std::array<unsigned char,3>{0x83,0xC4,0x40}){log("aim_hook refused: launch stack cleanup mismatch");return;}
    const hook::PatchOps ops{nullptr,probe::CopyCurrentProcess,Protect,Write,Flush};
    const auto r=hook::Install(ops,Site,static_cast<probe::Address>(reinterpret_cast<std::uintptr_t>(&Entry)),Original);
    char text[180];sprintf_s(text,"aim_hook installed=%u mismatch=%u restore_failed=%u",unsigned(r.installed),unsigned(r.mismatch),unsigned(r.restoreFailed));log(text);
}
void Report(hook::Logger log){char text[220];sprintf_s(text,"aim_totals applied=%llu refused=%llu failed=%llu restore_failed=%llu fault=%u",applied.load(),refused.load(),failed.load(),restoreFailed.load(),unsigned(fault.load()));log(text);}
}
