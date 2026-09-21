#pragma once
#include "RotationLogic.h"
#include "HookLogic.h"
namespace nvvr::stereo {
enum class CameraAction { Preserve, CaptureBaseline, ApplyEye };
// Site 0 follows A68C60's world update. Site 1 only rebuilds the matrix
// from the existing world transform, which can already contain the HMD pose.
inline CameraAction RouteCamera(unsigned site,hook::Kind kind,bool readable,bool eyeActive){
    if(site!=0 || kind!=hook::Kind::World || !readable)return CameraAction::Preserve;
    return eyeActive?CameraAction::ApplyEye:CameraAction::CaptureBaseline;
}
struct Eye {
    std::array<float,9> head{};
    std::array<float,3> headMetres{},eyeMetres{}; // NV camera-local basis
    std::array<float,4> projection{}; // OpenVR raw left/right/top/bottom
};
struct CameraBlock {
    probe::Transform world;
    float matrix[16];
    probe::Frustum frustum;
};
static_assert(sizeof(CameraBlock)==144);
inline CameraBlock Capture(const probe::Camera& camera){
    CameraBlock b{};b.world=camera.world;b.frustum=camera.frustum;
    std::memcpy(b.matrix,camera.worldToCamera,sizeof(b.matrix));return b;
}
inline bool Build(const probe::Camera& base,const Eye& eye,CameraBlock& out){
    for(float v:eye.projection)if(!std::isfinite(v))return false;
    const auto& p=eye.projection;
    if(!(p[0]<0 && p[1]>0 && p[2]<0 && p[3]>0))return false;
    if(base.frustum.ortho || !std::isfinite(base.frustum.nearPlane) ||
       !std::isfinite(base.frustum.farPlane) || base.frustum.nearPlane<=0 ||
       base.frustum.farPlane<=base.frustum.nearPlane)return false;
    std::array<float,9> rotation{};
    if(!rotation::Compose(base.world,eye.head,rotation))return false;
    std::array<float,3> displacement=eye.headMetres;
    for(unsigned row=0;row<3;++row)
        for(unsigned col=0;col<3;++col)displacement[row]+=eye.head[row*3+col]*eye.eyeMetres[col];
    std::array<float,3> position{};
    if(!rotation::Translate(base.world,displacement,position))return false;
    out=Capture(base);
    std::memcpy(out.world.rotation,rotation.data(),sizeof(out.world.rotation));
    std::memcpy(out.world.position,position.data(),sizeof(out.world.position));
    out.frustum.left=p[0];out.frustum.right=p[1];out.frustum.top=-p[2];out.frustum.bottom=-p[3];
    return true;
}
enum class WriteResult { Applied, Invalid, Unavailable, Restored, RestoreFailed };
inline WriteResult WriteEye(probe::Address address,const probe::Camera& base,const Eye& eye,void* context,rotation::WriteFn write){
    CameraBlock target{};
    if(!Build(base,eye,target))return WriteResult::Invalid;
    if(!write || address<0x10000 || address>UINT32_MAX-0xF8)return WriteResult::Unavailable;
    if(write(context,address+0x68,&target,sizeof(target)))return WriteResult::Applied;
    const auto original=Capture(base);
    return write(context,address+0x68,&original,sizeof(original))?WriteResult::Restored:WriteResult::RestoreFailed;
}
// All orchestration decisions are injected, including refusal and copy failures.
// The caller owns GPU references until Submit returns; Restore runs after EACH eye.
template<class Ops> bool Draw(bool enabled,Ops& ops){
    if(!enabled || !ops.Begin()){ops.Render();return false;}
    for(unsigned eye=0;eye<2;++eye){
        if(!ops.SetEye(eye)){ops.Restore();if(eye==0)ops.Render();return false;}
        ops.Render();
        const bool copied=ops.Copy(eye);
        const bool restored=ops.Restore();
        if(!copied || !restored)return false;
    }
    return ops.Submit();
}
}
