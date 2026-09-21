#pragma once
#include "RotationLogic.h"
#include "vr/Quaternion.h"
namespace nvvr::vr {
struct HeadPose {
    obvr::vr::Quaternion reference=obvr::vr::Quaternion::Identity();
    bool centered=false,valid=false;
    std::array<float,9> local{};
    obvr::NiPoint3 origin{};
    std::array<float,3> offset{};
    bool Update(bool tracked,const obvr::vr::Quaternion& q,bool recenter,const obvr::NiPoint3& position={}){
        valid=false;
        const float n=q.LengthSquared();
        if(!tracked || !std::isfinite(n) || n<0.000001f ||
           !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))return false;
        const auto unit=q.Normalized();
        if(!centered || recenter){reference=obvr::vr::YawOnly(unit);origin=position;centered=true;}
        const auto displacement=obvr::vr::Rotate(reference.Conjugate(),position-origin);
        offset={-displacement.z,displacement.y,displacement.x};
        const auto relative=(reference.Conjugate()*unit).Normalized();
        // Camera-local columns are forward/up/right. OpenVR is right/up/back.
        // Proper basis change (-Z,Y,X), unlike OBVR's world-basis conversion.
        const obvr::vr::Quaternion camera{-relative.z,relative.y,relative.x,relative.w};
        const auto matrix=obvr::vr::ToMatrix(camera);
        std::memcpy(local.data(),matrix.data,sizeof(local));valid=true;return true;
    }
};
}
