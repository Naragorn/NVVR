#pragma once
#include "vr/OpenVRBackend.h"
#include <cmath>
namespace nvvr::ui {
enum class AnchorAction { Keep, Place, Wait };
inline AnchorAction PlanAnchor(bool anchored,bool poseAvailable,float distanceSquared){
    if(!poseAvailable)return anchored?AnchorAction::Keep:AnchorAction::Wait;
    if(!std::isfinite(distanceSquared)||distanceSquared<0)return AnchorAction::Wait;
    return anchored&&distanceSquared<=4.0f?AnchorAction::Keep:AnchorAction::Place;
}
template<class Set> bool AnchorRoom(bool& anchored,obvr::vr::openvr::HmdMatrix34& saved,
                                     const obvr::vr::openvr::HmdMatrix34* current,float distance,Set set){
    const auto action=PlanAnchor(anchored,current!=nullptr,current?obvr::vr::PoseDistanceSq(*current,saved):0);
    if(action==AnchorAction::Keep)return true;
    if(action==AnchorAction::Wait)return false;
    auto candidate=*current;
    obvr::vr::LevelPose(candidate);
    const auto placed=obvr::vr::OverlayPoseAhead(candidate,distance);
    if(set(placed)!=0)return false;
    // Commit only after the runtime accepts an absolute transform. Otherwise
    // the next frame must retry instead of retaining the initial HMD transform.
    saved=candidate;anchored=true;return true;
}
}
