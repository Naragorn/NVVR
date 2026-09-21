#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Headset.h"
#include "vr/OpenVRBackend.h"
#include "platform/PluginPath.h"
#include "render/HudLayer.h"
#include "FramePair.h"
#include <mutex>
#include <cstdarg>
#include <cstdio>
#include <cwchar>
namespace {
nvvr::hook::Logger logger=nullptr;
obvr::vr::OpenVRBackend backend;
obvr::render::HudLayer hud;
obvr::render::HudLayer cinema(true);
nvvr::vr::HeadPose pose;
std::mutex poseMutex;
bool tried=false,lastValid=false;
HMODULE preloadedClient=nullptr;
}
namespace obvr::log {
void Write(const char* format,...){char text[1024];va_list args;va_start(args,format);vsnprintf_s(text,sizeof(text),_TRUNCATE,format,args);va_end(args);if(logger)logger(text);}
}
namespace nvvr::vr {
void Configure(hook::Logger log){logger=log;obvr::platform::SetPluginModule(GetModuleHandleW(L"NVVR.dll"));}
bool PreloadClient(){
    if(preloadedClient)return true;
    wchar_t path[MAX_PATH]{};
    const auto size=GetModuleFileNameW(GetModuleHandleW(L"NVVR.dll"),path,MAX_PATH);
    if(!size||size>=MAX_PATH)return false;
    auto* slash=std::wcsrchr(path,L'\\');if(!slash)return false;
    if(wcscpy_s(slash+1,MAX_PATH-static_cast<std::size_t>(slash+1-path),L"openvr_api.dll")!=0)return false;
    preloadedClient=LoadLibraryW(path);
    if(logger)logger(preloadedClient?"OpenVR client preloaded before DXVK Vulkan extension query":"OpenVR client preload failed: openvr_api.dll missing/unloadable");
    return preloadedClient!=nullptr;
}
bool Poll(bool recenter){
    if(recenter){hud.ResetAnchor();cinema.ResetAnchor();}
    if(!tried){tried=true;backend.Start(true);}
    obvr::vr::Quaternion orientation{};obvr::NiPoint3 position{};
    const bool tracked=backend.IsRunning() && backend.ReadHeadPose(orientation,position);
    bool valid;
    {std::lock_guard<std::mutex> lock(poseMutex);valid=pose.Update(tracked,orientation,recenter,position);}
    if(valid!=lastValid){if(logger)logger(valid?"headset_pose valid: OpenVR head orientation available":"headset_pose lost: unmodified game camera fallback");lastValid=valid;}
    return valid;
}
bool Pose(std::array<float,9>& local,std::array<float,3>& offset){std::lock_guard<std::mutex> lock(poseMutex);if(!pose.valid)return false;local=pose.local;offset=pose.offset;return true;}
void Stop(){cinema.Close(backend);hud.Close(backend);backend.Stop();tried=false;lastValid=false;std::lock_guard<std::mutex> lock(poseMutex);pose=HeadPose{};}
void* BeginHud(void* device,unsigned frame){return hud.BeginCapture(device,frame);}
void EndHud(){hud.EndCapture();}
void SubmitHud(void* device){hud.Submit(backend,device,hud.HasCapture(),1.5f,1.6f,true,false);}
void ResetHud(){cinema.Close(backend);hud.Close(backend);}
bool CaptureCinema(void* device){return cinema.CaptureScreen(device);}
void SubmitLayers(void* device,bool flat){hud.Submit(backend,device,!flat&&hud.HasCapture(),1.5f,1.6f,true,false);cinema.Submit(backend,device,flat&&cinema.HasCapture(),1.5f,1.6f,true,false);}
bool BeginStereo(stereo::Eye (&eyes)[2],obvr::vr::openvr::HmdMatrix34& renderPose){
    if(!backend.IsSceneApplication() || !stereo::WaitUnderQueue(obvr::render::GetGameDevice(),[]{return backend.WaitGetPoses();}))return false;
    obvr::vr::Quaternion orientation{};obvr::NiPoint3 position{};
    if(!backend.GetRenderPose(orientation,position)||!backend.GetRenderPoseMatrix(renderPose))return false;
    std::lock_guard<std::mutex> lock(poseMutex);
    if(!pose.Update(true,orientation,false,position))return false;
    for(int i=0;i<2;++i){
        auto& eye=eyes[i];eye.head=pose.local;eye.headMetres=pose.offset;
        auto& p=eye.projection;obvr::NiPoint3 eyeOffset{};
        if(!backend.GetEyeProjection(i,p[0],p[1],p[2],p[3]) || !backend.GetEyeOffset(i,eyeOffset))return false;
        eye.eyeMetres={-eyeOffset.z,eyeOffset.y,eyeOffset.x};
    }
    return true;
}
bool SubmitStereo(void* left,void* right,const obvr::vr::openvr::HmdMatrix34& renderPose){
    const int l=backend.SubmitEye(0,left,obvr::vr::openvr::kTextureTypeVulkan,nullptr,&renderPose);
    const int r=backend.SubmitEye(1,right,obvr::vr::openvr::kTextureTypeVulkan,nullptr,&renderPose);
    static int lastLeft=-1,lastRight=-1;
    if(l!=lastLeft || r!=lastRight){char text[128];sprintf_s(text,"stereo_submit left=%d right=%d",l,r);if(logger)logger(text);lastLeft=l;lastRight=r;}
    return l==0 && r==0;
}
}
