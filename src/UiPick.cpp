#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "UiPick.h"
#include "UiSizeLogic.h"
#include "ProcessReader.h"
#include "Rotation.h"
#include <cstdio>
namespace nvvr::ui {
namespace {
constexpr probe::Address Site=0x00712926,Original=0x00A71080,RayOriginal=0x00A703A0;
using Native=bool(__thiscall*)(void*,unsigned,unsigned,void*,void*,void*);
using Ray=void(__thiscall*)(const void*,float,float,void*,void*);
Native native=reinterpret_cast<Native>(Original);
Ray ray=reinterpret_cast<Ray>(RayOriginal);
thread_local PickLayout layout{};
hook::Logger logger=nullptr;
#ifdef NVVR_UI_PICK_TESTS
PickPlan testPlan;
#endif
bool __fastcall Pick(void* camera,void*,unsigned x,unsigned y,void* origin,void* direction,void* target){
    __declspec(align(16)) unsigned char state[512];
    __asm fxsave state
    PickPlan plan;
#ifdef NVVR_UI_PICK_TESTS
    plan=testPlan;
#else
    probe::Camera data{};
    const auto address=static_cast<probe::Address>(reinterpret_cast<std::uintptr_t>(camera));
    const probe::Reader reader{nullptr,probe::CopyCurrentProcess};
    if(reader.Value(address,0,data))
        plan=PlanPick(rotation::HeadEnabled(),address,data,layout,sourceSize,x,y,target!=nullptr);
    static unsigned reports=0;
    if(logger&&reports<12&&plan.route==PickRoute::Menu){
        ++reports;char text[240];
        sprintf_s(text,"ui_pick source=%ux%u cursor=%u,%u logical=%.3f,%.3f layout=%.3fx%.3f camera=%08X",
            sourceSize.width,sourceSize.height,x,y,plan.x,plan.y,layout.width,layout.height,address);logger(text);
    }
#endif
    __asm fxrstor state
    return ExecutePick(plan,[&]{return native(camera,x,y,origin,direction,target);},
        [&](const probe::Camera& adjusted,float px,float py){ray(&adjusted,px,py,origin,direction);});
}
bool Protect(void*,probe::Address address,std::uint32_t protection,std::uint32_t* previous){
    DWORD old=0;const bool ok=VirtualProtect(reinterpret_cast<void*>(address),5,protection,&old)!=0;*previous=old;return ok;
}
void Write(void*,probe::Address address,const void* bytes,std::size_t size){std::memcpy(reinterpret_cast<void*>(address),bytes,size);}
bool Flush(void*,probe::Address address){return FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),5)!=0;}
}
void SetPickLayout(PickLayout value){layout=value;}
void InstallPickHook(hook::Logger log){
    logger=log;
    const hook::PatchOps ops{nullptr,probe::CopyCurrentProcess,Protect,Write,Flush};
    const auto r=hook::Install(ops,Site,static_cast<probe::Address>(reinterpret_cast<std::uintptr_t>(&Pick)),Original);
    char text[180];sprintf_s(text,"ui_pick_hook site=%08X installed=%u restore_failed=%u flush_failed=%u",
        Site,unsigned(r.installed),unsigned(r.restoreFailed),unsigned(r.flushFailed));log(text);
}
#ifdef NVVR_UI_PICK_TESTS
bool TestPick(void* camera,unsigned x,unsigned y,void* origin,void* direction,void* target,
              PickNative original,PickRay cast,const PickPlan& plan){
    native=original;ray=cast;testPlan=plan;return Pick(camera,nullptr,x,y,origin,direction,target);
}
#endif
}
