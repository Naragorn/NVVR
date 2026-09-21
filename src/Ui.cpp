#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Ui.h"
#include "UiSize.h"
#include "UiPick.h"
#include "UiSizeLogic.h"
#include "UiLogic.h"
#include "MenuGeometry.h"
#include "PresentationLogic.h"
#include "Headset.h"
#include "Stereo.h"
#include "Rotation.h"
#include "ProcessReader.h"
#include "render/GameDevice.h"
#include "render/D3D9Types.h"
#include "render/D3D11Types.h"
#include <atomic>
#include <cstdio>
namespace nvvr::ui {
namespace {
namespace gpu=obvr::render;
using InterfaceFn=void(__thiscall*)(void*,void*,bool);
using ResetFn=SInt32(__stdcall*)(void*,void*);
constexpr probe::Address Site=0x007144D3,Original=0x007134D0;
const probe::Reader reader{nullptr,probe::CopyCurrentProcess};
gpu::d3d9::SetRenderTargetFn originalTarget=nullptr;
gpu::d3d9::SetViewportFn originalViewport=nullptr;
thread_local resolution::Size captureTargetExtent{};
gpu::d3d9::SetDepthStencilSurfaceFn originalDepth=nullptr;
gpu::d3d9::SetRenderStateFn originalState=nullptr;
ResetFn originalReset=nullptr;
gpu::d3d9::PresentFn originalPresent=nullptr;
hook::Logger logger=nullptr;
bool presentHook=false;
thread_local bool insidePresent=false,uiCaptured=false;
std::atomic<std::uint64_t> cinemaFrames{0},cinemaFailures{0},devicePresents{0};
void* hookedTable=nullptr;
std::atomic<bool> ready{false},fault{false};
thread_local void* capturingDevice=nullptr;
thread_local void* captureSurface=nullptr;
thread_local RequestedStates requested;
std::atomic<unsigned> frame{0};
std::atomic<std::uint64_t> captures{0},fallbacks{0},resets{0};
SInt32 __stdcall SetViewport(void* device,const gpu::d3d9::Viewport* requestedViewport){
    if(!requestedViewport)return originalViewport(device,requestedViewport);
    auto v=*requestedViewport;
    if(FitViewport(capturingDevice==device,v.x,v.y,v.width,v.height,captureTargetExtent,captureExtent)){
        v.width=captureExtent.width;v.height=captureExtent.height;
    }
        return originalViewport(device,&v);
}
SInt32 __stdcall SetTarget(void* device,UInt32 index,void* surface){
    const bool replaced=capturingDevice==device&&index==0;
    const auto result=originalTarget(device,index,replaced?captureSurface:surface);
    if(replaced&&!gpu::d3d11::Failed(result)&&originalViewport){
        const gpu::d3d9::Viewport v{0,0,captureExtent.width,captureExtent.height,0,1};
        const auto adjusted=originalViewport(device,&v);if(gpu::d3d11::Failed(adjusted))return adjusted;
    }
    return result;
}
SInt32 __stdcall SetDepth(void* device,void* surface){return originalDepth(device,capturingDevice==device?nullptr:surface);}
SInt32 __stdcall SetState(void* device,UInt32 state,UInt32 value){if(capturingDevice==device)requested.Remember(state,value);return originalState(device,state,RenderState(capturingDevice==device,state,value));}
void FinishDeviceFrame(){
    __declspec(align(16)) unsigned char state[512];
    __asm fxsave state
    ++devicePresents;
    rotation::Poll(logger);
    Present();
    __asm fxrstor state
}
SInt32 __stdcall PresentDevice(void* device,const gpu::d3d9::Rect* source,const gpu::d3d9::Rect* dest,void* window,const void* dirty){
    return presentation::Present(device==gpu::GetGameDevice(),insidePresent,FinishDeviceFrame,
        [&]{return originalPresent(device,source,dest,window,dirty);});
}
SInt32 __stdcall ResetDevice(void* device,void* params){++resets;stereo::Reset();vr::ResetHud();return originalReset(device,params);}
template<class Fn> bool Slot(void** table,unsigned index,Fn replacement,Fn& original){
    auto* slot=table+index;original=reinterpret_cast<Fn>(*slot);
    DWORD old=0;if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&old))return false;
    void* observed=InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(replacement),reinterpret_cast<void*>(original));
    DWORD unused=0;const bool restored=VirtualProtect(slot,sizeof(void*),old,&unused)!=0;
    return observed==reinterpret_cast<void*>(original)&&restored;
}
struct CaptureOps {
    void* object;void* culler;bool pipboy;
    void* device=nullptr;void* savedTarget=nullptr;void* savedDepth=nullptr;
    gpu::d3d9::Viewport viewport{};
    bool saved=false,menuWritten=false;
    probe::Address menuCamera=0;
    probe::Camera menuSaved{},menuAdjusted{};
    probe::Frustum menuFrustum{};
    resolution::Size menuSize{},targetSize{};
    ~CaptureOps(){gpu::d3d11::Release(savedTarget);gpu::d3d11::Release(savedDepth);}
    bool Save(){
        device=gpu::GetGameDevice();if(!device)return false;
        const auto getTarget=gpu::d3d9::Method<gpu::d3d9::GetRenderTargetFn>(device,gpu::d3d9::kDeviceGetRenderTarget);
        const auto getDepth=gpu::d3d9::Method<gpu::d3d9::GetDepthStencilSurfaceFn>(device,gpu::d3d9::kDeviceGetDepthStencilSurface);
        const auto getViewport=gpu::d3d9::Method<gpu::d3d9::GetViewportFn>(device,gpu::d3d9::kDeviceGetViewport);
        if(!getTarget||!getDepth||!getViewport)return false;
        if(gpu::d3d11::Failed(getTarget(device,0,&savedTarget))||!savedTarget)return false;
        // D3DERR_NOTFOUND means no depth attachment, which is a valid UI state.
        const auto depth=getDepth(device,&savedDepth);
        if(gpu::d3d11::Failed(depth)&&static_cast<UInt32>(depth)!=0x88760866u)return false;
        if(gpu::d3d11::Failed(getViewport(device,&viewport)))return false;
        const auto getState=gpu::d3d9::Method<gpu::d3d9::GetRenderStateFn>(device,gpu::d3d9::kDeviceGetRenderState);
        if(!getState)return false;
        for(unsigned i=0;i<ChangedStates.size();++i)if(gpu::d3d11::Failed(getState(device,ChangedStates[i],&requested.values[i])))return false;
        static unsigned observations=0;
        if(logger && (observations<8 || frame.load()==120 || frame.load()==600)){
            ++observations;
            probe::Address scene=0,camera=0;probe::Camera data{};
            const bool cameraRead=reader.Value(static_cast<probe::Address>(reinterpret_cast<std::uintptr_t>(object)),4,scene)&&
                reader.Value(scene,0xAC,camera)&&reader.Value(camera,0,data);
            gpu::d3d9::SurfaceDesc desc{};
            const auto describe=gpu::d3d9::Method<gpu::d3d9::GetDescFn>(savedTarget,gpu::d3d9::kSurfaceGetDesc);
            const bool targetRead=describe&&!gpu::d3d11::Failed(describe(savedTarget,&desc));
            char text[640];
            sprintf_s(text,"ui_capture_geometry frame=%u camera_read=%u scene=%08X camera=%08X vtable=%08X frustum=(%.6f,%.6f,%.6f,%.6f) ortho=%u viewport=(%u,%u,%u,%u) target_read=%u target_size=%ux%u",frame.load(),unsigned(cameraRead),scene,camera,data.vtable,data.frustum.left,data.frustum.right,data.frustum.top,data.frustum.bottom,unsigned(data.frustum.ortho),viewport.x,viewport.y,viewport.width,viewport.height,unsigned(targetRead),desc.width,desc.height);
            logger(text);
            probe::Address root=0,values=0;unsigned count=0;
            const bool rootRead=reader.Value(static_cast<probe::Address>(reinterpret_cast<std::uintptr_t>(object)),0x9C,root)&&reader.Value(root,0x14,values)&&reader.Value(root,0x18,count);
            float width=-1,height=-1;
            if(rootRead&&count<=256){for(unsigned i=0;i<count;++i){probe::Address value=0;unsigned id=0;float number=0;
                if(reader.Value(values,i*4,value)&&reader.Value(value,0,id)&&reader.Value(value,8,number)){
                    if(id==0xFB1)width=number;if(id==0xFB0)height=number;
                }
            }}
            sprintf_s(text,"ui_layout_geometry frame=%u root_read=%u root=%08X values=%u width=%.6f height=%.6f",frame.load(),unsigned(rootRead),root,count,width,height);logger(text);
        }
        saved=true;return true;
    }
    bool Begin(){
        probe::Address scene=0;float menuWidth=0,menuHeight=0;
        const auto objectAddress=static_cast<probe::Address>(reinterpret_cast<std::uintptr_t>(object));
        const auto renderer=probe::ReadRenderer(reader);
        if(renderer.state!=probe::State::Ready||!ReadMenuSize(reader,objectAddress,menuWidth,menuHeight)||
           !reader.Value(objectAddress,4,scene)||!reader.Value(scene,0xAC,menuCamera)||
           !reader.Value(menuCamera,0,menuSaved)||menuSaved.vtable!=probe::CameraVtable||
           !MenuCamera(menuSaved,menuWidth,menuHeight,menuAdjusted))return false;
        menuFrustum=menuAdjusted.frustum;
        SetPickLayout({menuCamera,menuWidth,menuHeight});
        targetSize={renderer.width,renderer.height};menuSize=FitMenu(menuWidth,menuHeight,targetSize);
        if(!menuSize.width||!menuSize.height)return false;
        captureExtent=menuSize;captureTargetExtent=targetSize;
        auto* surface=vr::BeginHud(device,frame.load());if(!surface)return false;
        captureSurface=surface;capturingDevice=device;
        if(gpu::d3d11::Failed(originalDepth(device,nullptr)) || gpu::d3d11::Failed(originalTarget(device,0,surface))){End();return false;}
        const gpu::d3d9::Viewport v{0,0,menuSize.width,menuSize.height,0,1};
        if(!originalViewport||gpu::d3d11::Failed(originalViewport(device,&v))){End();return false;}
        menuWritten=true;
        if(!ApplyMenuCamera(menuCamera,menuAdjusted,
            [](probe::Address address,const void* bytes,std::size_t size){return rotation::WriteCurrent(nullptr,address,bytes,size);},
            [](probe::Address camera){reinterpret_cast<void(__thiscall*)(void*)>(hook::Original)(reinterpret_cast<void*>(camera));})){
            End();return false;
        }
        static bool reported=false;
        if(!reported&&logger){reported=true;char text[256];sprintf_s(text,"ui_layout_fit camera=%.3fx%.3f layout=%.3fx%.3f content=%ux%u target=%ux%u ui_source=%ux%u",menuSaved.frustum.right,menuSaved.frustum.top,menuFrustum.right,menuFrustum.top,menuSize.width,menuSize.height,targetSize.width,targetSize.height,sourceSize.width,sourceSize.height);logger(text);}
        return true;
    }
    void Render(){reinterpret_cast<InterfaceFn>(Original)(object,culler,pipboy);}
    void End(){
        capturingDevice=nullptr;captureSurface=nullptr;vr::EndHud();
        if(menuWritten){
            menuWritten=false;
            if(!RestoreMenuCamera(menuCamera,menuSaved,
                [](probe::Address address,const void* bytes,std::size_t size){return rotation::WriteCurrent(nullptr,address,bytes,size);}))fault=true;
        }
        // Restore the game's latest requests, including depth testing, so its
        // render-state cache agrees with the device after our forced values.
        for(unsigned i=0;i<ChangedStates.size();++i)if(gpu::d3d11::Failed(originalState(device,ChangedStates[i],requested.values[i])))fault=true;
    }
    bool Restore(){
        if(!saved)return false;
        const auto setViewport=gpu::d3d9::Method<gpu::d3d9::SetViewportFn>(device,gpu::d3d9::kDeviceSetViewport);
        const bool targetOk=!gpu::d3d11::Failed(originalTarget(device,0,savedTarget));
        const bool depthOk=!gpu::d3d11::Failed(originalDepth(device,savedDepth));
        const bool viewportOk=setViewport&&!gpu::d3d11::Failed(setViewport(device,&viewport));
        if(!targetOk||!depthOk||!viewportOk){fault=true;return false;}return true;
    }
};
void __fastcall RenderInterface(void* object,void*,void* culler,bool pipboy){
    CaptureOps ops{object,culler,pipboy};
    const bool enabled=rotation::HeadEnabled()&&ready.load()&&!fault.load()&&!capturingDevice&&!stereo::FlatRequested();
    if(Capture(enabled,ops)){++captures;uiCaptured=true;}else ++fallbacks;
}
bool Protect(void*,probe::Address address,std::uint32_t protection,std::uint32_t* previous){DWORD old=0;const bool ok=VirtualProtect(reinterpret_cast<void*>(address),5,protection,&old)!=0;*previous=old;return ok;}
void Write(void*,probe::Address address,const void* bytes,std::size_t size){std::memcpy(reinterpret_cast<void*>(address),bytes,size);}
bool Flush(void*,probe::Address address){return FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),5)!=0;}
}
void EnsureDevice(hook::Logger log){
    auto* device=gpu::GetGameDevice();if(!device||gpu::IdentifyDevice(device)!=gpu::DeviceKind::Dxvk)return;
    void** table=*static_cast<void***>(device);if(table==hookedTable)return;
    if(hookedTable){log("ui_device refused: replacement device uses different vtable");fault=true;return;}
    hookedTable=table;
    const bool viewport=Slot(table,gpu::d3d9::kDeviceSetViewport,SetViewport,originalViewport);
    const bool target=Slot(table,gpu::d3d9::kDeviceSetRenderTarget,SetTarget,originalTarget);
    const bool depth=Slot(table,gpu::d3d9::kDeviceSetDepthStencilSurface,SetDepth,originalDepth);
    const bool state=Slot(table,gpu::d3d9::kDeviceSetRenderState,SetState,originalState);
    const bool reset=Slot(table,16,ResetDevice,originalReset);
    presentHook=Slot(table,gpu::d3d9::kDevicePresent,PresentDevice,originalPresent);
    ready=target&&depth&&state&&reset&&viewport;
    log(presentHook?"device_present hook ready: frame completion includes movies":"device_present hook unavailable: NVSE completion fallback");
    log(ready?"ui_device hooks ready: target/depth/alpha/reset":"ui_device hooks incomplete: capture disabled");
}
void Install(hook::Logger log){
    InstallSizeHooks(log);
    InstallPickHook(log);
    logger=log;
    const hook::PatchOps ops{nullptr,probe::CopyCurrentProcess,Protect,Write,Flush};
    const auto r=hook::Install(ops,Site,static_cast<probe::Address>(reinterpret_cast<std::uintptr_t>(&RenderInterface)),Original);
    char text[180];sprintf_s(text,"ui_hook installed=%u mismatch=%u restore_failed=%u",unsigned(r.installed),unsigned(r.mismatch),unsigned(r.restoreFailed));log(text);
}
bool HasPresentHook(){return presentHook;}
void Present(){
    auto* device=gpu::GetGameDevice();
    const bool flat=presentation::Cinema(stereo::FlatRequested(),stereo::HasSubmitted(),uiCaptured);
    if(rotation::HeadEnabled() && flat){if(vr::CaptureCinema(device))++cinemaFrames;else ++cinemaFailures;}
    stereo::Present();
    if(rotation::HeadEnabled())vr::SubmitLayers(device,flat);
    uiCaptured=false;++frame;
}
void Report(hook::Logger log){char text[200];sprintf_s(text,"ui_totals captures=%llu fallback=%llu resets=%llu ready=%u fault=%u",captures.load(),fallbacks.load(),resets.load(),unsigned(ready.load()),unsigned(fault.load()));log(text);sprintf_s(text,"presentation_totals device_presents=%llu cinema_frames=%llu cinema_failures=%llu",devicePresents.load(),cinemaFrames.load(),cinemaFailures.load());log(text);}
}
