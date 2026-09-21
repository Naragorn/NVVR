#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Rotation.h"
#include "Headset.h"
#include "VrControls.h"
static_assert(nvvr::rotation::RecenterKey==VK_DELETE);
#include <atomic>
#include <cstdio>
namespace nvvr::rotation {
namespace {
Toggle toggle; // Only accessed from the serialized NVSE message callback.
AlwaysVr headControl;

std::atomic<bool> headEnabled{false};
std::atomic<bool> enabled{false},fault{false};
std::atomic<DWORD> inputThread{0};
std::atomic<std::uint64_t> applied{0},refused{0},writeFailures{0},restoreFailures{0};
}
bool WriteCurrent(void*,probe::Address address,const void* bytes,std::size_t size) {
    SIZE_T copied=0;
    return WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),bytes,size,&copied)!=0 && copied==size;
}
bool HeadEnabled(){return headEnabled.load()&&!fault.load()&&GetCurrentThreadId()==inputThread.load();}
void Poll(hook::Logger log){
    DWORD process=0;const auto foreground=GetForegroundWindow();
    const bool focused=foreground && GetWindowThreadProcessId(foreground,&process)!=0 && process==GetCurrentProcessId();
    const bool pressed=(GetAsyncKeyState(VK_F8)&0x8000)!=0;
    const bool previous=toggle.enabled;
    toggle.Step(pressed,focused,fault.load());
    const bool headPrevious=headControl.enabled;
    const auto action=headControl.Step(focused,fault.load(),(GetAsyncKeyState(RecenterKey)&0x8000)!=0);
    if(action.poll)nvvr::vr::Poll(action.recenter);
    else if(action.stop)nvvr::vr::Stop();
    headEnabled=headControl.enabled;
    if(headPrevious!=headControl.enabled)log(headControl.enabled?"seated_vr enabled automatically; Delete recenter; 6DoF/stereo/head aim/spatial UI":"head_tracking disabled: camera fault");
    inputThread=GetCurrentThreadId();enabled=toggle.enabled;
    if(previous!=toggle.enabled)log(toggle.enabled?"rotation_test enabled: F8, local-up yaw +10 degrees":"rotation_test disabled: F8/focus/fault; fresh engine update restores baseline");
}
void Reset(hook::Logger log){
    const bool was=enabled.exchange(false);toggle.Reset();headControl.Reset();headEnabled=false;nvvr::vr::Stop();
    if(was)log("rotation_test disabled: lifecycle reset; fresh engine update restores baseline");
}
Result ForCall(unsigned site,probe::Address camera,const hook::Sample& snapshot,const probe::Reader& reader){
    if((!enabled.load()&&!headEnabled.load()) || fault.load())return Result::Disabled;
    if(GetCurrentThreadId()!=inputThread.load()){++refused;return Result::OtherPath;}
    std::array<float,9> head{};
    std::array<float,3> offset{};
    const bool useHead=headEnabled.load();
    if(useHead&&!nvvr::vr::Pose(head,offset))return Result::Disabled;
    const auto result=Apply(true,site,camera,snapshot,reader,nullptr,WriteCurrent,useHead?&head:nullptr,useHead?&offset:nullptr);
    if(result==Result::Applied)++applied;
    else if(result==Result::WriteFailedRestored || result==Result::RestoreFailed){
        ++writeFailures;if(result==Result::RestoreFailed)++restoreFailures;fault=true;enabled=false;
    }else ++refused;
    return result;
}
void Report(hook::Logger log){char text[256];
    sprintf_s(text,"rotation_totals enabled=%u head_enabled=%u fault=%u applied=%llu refused=%llu write_failures=%llu restore_failures=%llu",
        unsigned(enabled.load()),unsigned(headEnabled.load()),unsigned(fault.load()),applied.load(),refused.load(),writeFailures.load(),restoreFailures.load());log(text);
}
}
