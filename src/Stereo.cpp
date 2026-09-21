#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Stereo.h"
#include "StereoLogic.h"
#include "FramePair.h"
#include "PresentationLogic.h"
#include "Headset.h"
#include "Rotation.h"
#include "ProcessReader.h"
#include "render/GameFrame.h"
#include "render/D3D11Types.h"
#include <cstdio>
#include <atomic>
namespace nvvr::stereo {
namespace {
namespace gpu=obvr::render;
constexpr probe::Address RenderOriginal=0x008706B0;
constexpr probe::Address RenderSites[]={0x00870244,0x008702A9};
using RenderFn=void(__thiscall*)(void*,void*,int,int);
using SceneFn=void(__thiscall*)(void*,void*,int);
constexpr probe::Address SceneSites[]={0x0087074B,0x0087075E};
constexpr probe::Address SceneOriginals[]={0x00870A00,0x00870BD0};
thread_local bool sceneInside=false;
using MatrixFn=void(__thiscall*)(void*);
const probe::Reader reader{nullptr,probe::CopyCurrentProcess};
thread_local probe::Address baseAddress=0;
thread_local probe::Camera baseCamera{};
thread_local bool haveBase=false,active=false,inside=false;
thread_local bool submitted=false,flatRequested=false;
thread_local Eye currentEye{};
std::atomic<bool> fault{false};
std::atomic<std::uint64_t> calls{0},pairs{0},fallbacks{0},copyFailures{0},writeFailures{0};
FramePair frames;
hook::Logger projectionLogger=nullptr;
unsigned projectionReports=0;
struct DrawOps {
    void* object;void* target;int mode;SceneFn scene;
    probe::Address camera=0;
    probe::Camera baseline{};
    CameraBlock saved{};
    Eye eyes[2]{},rawEyes[2]{};
    Placement placement[2]{};
    obvr::vr::openvr::HmdMatrix34 pose{};
    bool Begin(){
        if(!haveBase || !frames.Begin(gpu::GetGameDevice()))return false;
        const auto sample=probe::ReadCamera(reader);
        probe::Address collision=0;
        if(sample.state!=probe::State::Ready || sample.camera!=baseAddress ||
           !reader.Value(sample.camera,0x1C,collision)||collision)return false;
        camera=sample.camera;baseline=baseCamera;saved=Capture(sample.data);
        if(!vr::BeginStereo(rawEyes,pose)||!PreparePair(rawEyes,eyes,placement))return false;
        CameraBlock check{};
        return Build(baseline,eyes[0],check)&&Build(baseline,eyes[1],check);
    }
    bool SetEye(unsigned eye){
        currentEye=eyes[eye];active=true;
        const auto r=WriteEye(camera,baseline,currentEye,nullptr,rotation::WriteCurrent);
        if(r!=WriteResult::Applied){++writeFailures;fault=true;return false;}
        reinterpret_cast<MatrixFn>(hook::Original)(reinterpret_cast<void*>(camera));return true;
    }
    void Render(){scene(object,target,mode);}
    bool Copy(unsigned eye){
        // Bounded evidence at the capture boundary, not an assumed projection
        // from the earlier camera update. No writes or empirical IPD scaling.
        if(projectionLogger && projectionReports<24){
            ++projectionReports;
            auto* device=gpu::GetGameDevice();gpu::d3d9::Matrix4 matrix{};
            const auto get=gpu::d3d9::Method<gpu::d3d9::GetTransformFn>(device,gpu::d3d9::kDeviceGetTransform);
            const bool got=get&&!gpu::d3d11::Failed(get(device,gpu::d3d9::kTransformProjection,&matrix));
            const auto now=probe::ReadCamera(reader);
            const auto& requested=rawEyes[eye];
            char text[1200];const auto* m=reinterpret_cast<const float*>(&matrix);
            sprintf_s(text,"eye_capture eye=%u raw=(%.6f,%.6f,%.6f,%.6f) offset_m=(%.6f,%.6f,%.6f) camera_ready=%u camera=(%.5f,%.5f,%.5f) frustum=(%.6f,%.6f,%.6f,%.6f) gpu_projection_read=%u gpu=(%.6f,%.6f,%.6f,%.6f;%.6f,%.6f,%.6f,%.6f;%.6f,%.6f,%.6f,%.6f;%.6f,%.6f,%.6f,%.6f)",eye,requested.projection[0],requested.projection[1],requested.projection[2],requested.projection[3],requested.eyeMetres[0],requested.eyeMetres[1],requested.eyeMetres[2],unsigned(now.state==probe::State::Ready),now.data.world.position[0],now.data.world.position[1],now.data.world.position[2],now.data.frustum.left,now.data.frustum.right,now.data.frustum.top,now.data.frustum.bottom,unsigned(got),m[0],m[1],m[2],m[3],m[4],m[5],m[6],m[7],m[8],m[9],m[10],m[11],m[12],m[13],m[14],m[15]);
            projectionLogger(text);
        }
        if(fault.load() || !frames.Copy(eye,false,&placement[eye])){++copyFailures;return false;}return true;
    }
    bool Restore(){
        active=false;
        if(!rotation::WriteCurrent(nullptr,camera+0x68,&saved,sizeof(saved))){++writeFailures;fault=true;return false;}
        return true;
    }
    bool Submit(){submitted=true;return frames.Submit([&](void* left,void* right){return vr::SubmitStereo(left,right,pose);});}
};
void RenderScene(SceneFn scene,void* object,void* target,int mode){
    if(sceneInside){scene(object,target,mode);return;}
    sceneInside=true;
    DrawOps ops{object,target,mode,scene};
    if(Draw(inside&&rotation::HeadEnabled()&&!fault.load()&&target==nullptr&&!submitted&&!flatRequested,ops)){++pairs;submitted=true;}else ++fallbacks;
    frames.ReleaseReferences();sceneInside=false;
}
void __fastcall SceneA(void* object,void*,void* target,int mode){RenderScene(reinterpret_cast<SceneFn>(SceneOriginals[0]),object,target,mode);}
void __fastcall SceneB(void* object,void*,void* target,int mode){RenderScene(reinterpret_cast<SceneFn>(SceneOriginals[1]),object,target,mode);}
void __fastcall Render(void* object,void*,void* target,int a,int b){
    ++calls;
    if(inside){reinterpret_cast<RenderFn>(RenderOriginal)(object,target,a,b);return;}
    inside=true;
    if(rotation::HeadEnabled() && target==nullptr && presentation::FlatPass((a&255)!=0,presentation::MovieQueued(reader)))flatRequested=true;
    // Camera and menu routing execute once. Only the selected world block
    // (geometry, first person, image-space completion) draws the two eyes.
    reinterpret_cast<RenderFn>(RenderOriginal)(object,target,a,b);
    inside=false;
}
bool Protect(void*,probe::Address address,std::uint32_t protection,std::uint32_t* previous){DWORD old=0;const bool ok=VirtualProtect(reinterpret_cast<void*>(address),5,protection,&old)!=0;*previous=old;return ok;}
void Write(void*,probe::Address address,const void* bytes,std::size_t size){std::memcpy(reinterpret_cast<void*>(address),bytes,size);}
bool Flush(void*,probe::Address address){return FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),5)!=0;}
}
void Baseline(unsigned site,probe::Address camera,const hook::Sample& sample){
    if(RouteCamera(site,sample.kind,sample.before,active)==CameraAction::CaptureBaseline){baseAddress=camera;baseCamera=sample.a;haveBase=true;}
}
bool Override(unsigned site,probe::Address camera,const hook::Sample& sample){
    if(RouteCamera(site,sample.kind,sample.before,active)!=CameraAction::ApplyEye)return false;
    const auto r=WriteEye(camera,sample.a,currentEye,nullptr,rotation::WriteCurrent);
    if(r!=WriteResult::Applied){++writeFailures;fault=true;}
    return true;
}
#ifdef NVVR_HOOK_TESTS
bool TestBaseline(probe::Address& camera,probe::Camera& value){
    camera=baseAddress;value=baseCamera;return haveBase;
}
#endif
bool FlatRequested(){return flatRequested || presentation::MovieQueued(reader);}
bool HasSubmitted(){return submitted;}
void Reset(){active=false;haveBase=false;submitted=false;flatRequested=false;frames.Reset();}
void Present(){
    // Menus without a world pass still need a compositor frame. Their visible
    // content is the spatial UI layer; the eyes behind it are opaque black.
    if(!submitted && rotation::HeadEnabled()&&!fault.load()&&frames.Begin(gpu::GetGameDevice())){
        Eye eyes[2]{};obvr::vr::openvr::HmdMatrix34 pose{};
        if(vr::BeginStereo(eyes,pose)&&frames.Copy(0,true)&&frames.Copy(1,true))frames.Submit([&](void* left,void* right){return vr::SubmitStereo(left,right,pose);});
        frames.ReleaseReferences();
    }
    submitted=false;flatRequested=false;
}
bool Gaze(probe::Transform& world){
    if(!haveBase||fault.load()||!rotation::HeadEnabled())return false;
    const auto sample=probe::ReadCamera(reader);
    if(sample.state!=probe::State::Ready||sample.camera!=baseAddress)return false;
    std::array<float,9> head{};std::array<float,3> offset{},position{};
    if(!vr::Pose(head,offset))return false;
    std::array<float,9> composed{};
    if(!rotation::Compose(baseCamera.world,head,composed)||!rotation::Translate(baseCamera.world,offset,position))return false;
    world=baseCamera.world;std::memcpy(world.rotation,composed.data(),36);std::memcpy(world.position,position.data(),12);return true;
}
void Install(hook::Logger log){
    projectionLogger=log;
    for(auto site:RenderSites){std::array<unsigned char,5> actual{};
        if(!reader.Read(site,0,actual.data(),actual.size())||actual!=hook::Call(site,RenderOriginal)){log("stereo_hooks refused: render CALL mismatch");return;}}
    for(unsigned i=0;i<2;++i){std::array<unsigned char,5> actual{};
        if(!reader.Read(SceneSites[i],0,actual.data(),actual.size())||actual!=hook::Call(SceneSites[i],SceneOriginals[i])){log("scene_hooks refused: world block CALL mismatch");return;}}
    const hook::PatchOps ops{nullptr,probe::CopyCurrentProcess,Protect,Write,Flush};
    const std::uintptr_t sceneHooks[]={reinterpret_cast<std::uintptr_t>(&SceneA),reinterpret_cast<std::uintptr_t>(&SceneB)};
    for(unsigned i=0;i<2;++i){const auto r=hook::Install(ops,SceneSites[i],static_cast<probe::Address>(sceneHooks[i]),SceneOriginals[i]);
        char text[180];sprintf_s(text,"world_block_hook site=%08X original=%08X installed=%u",SceneSites[i],SceneOriginals[i],unsigned(r.installed));log(text);}
    for(auto site:RenderSites){const auto r=hook::Install(ops,site,static_cast<probe::Address>(reinterpret_cast<std::uintptr_t>(&Render)),RenderOriginal);
        char text[256];sprintf_s(text,"stereo_hook site=%08X installed=%u restore_failed=%u flush_failed=%u",site,unsigned(r.installed),unsigned(r.restoreFailed),unsigned(r.flushFailed));log(text);}
}
void Report(hook::Logger log){char text[256];sprintf_s(text,"stereo_totals calls=%llu pairs=%llu fallback=%llu copy_failures=%llu write_failures=%llu fault=%u",calls.load(),pairs.load(),fallbacks.load(),copyFailures.load(),writeFailures.load(),unsigned(fault.load()));log(text);}
}
