#pragma once
#include "MenuGeometry.h"
namespace nvvr::ui {
struct PickLayout { probe::Address camera=0; float width=0,height=0; };
enum class PickRoute { Native, Miss, Menu };
struct PickPlan { PickRoute route=PickRoute::Native; probe::Camera camera{}; float x=0,y=0; };
inline PickPlan PlanPick(bool enabled,probe::Address address,const probe::Camera& camera,
                         PickLayout layout,resolution::Size source,unsigned x,unsigned y,bool customTarget){
    PickPlan p;
    if(!enabled||customTarget||address!=layout.camera||!address||
       camera.vtable!=probe::CameraVtable||!resolution::Valid(source)||
       !MenuCamera(camera,layout.width,layout.height,p.camera))return p;
    if(x>source.width||y>source.height){p.route=PickRoute::Miss;return p;}
    // A6FA30 divides by the active render target, which is taller than the
    // captured menu. Use the same source rectangle as the UI size readers.
    p.x=float(double(x)*layout.width/source.width);
    p.y=float((1.0-double(y)/source.height)*layout.height);
    p.route=PickRoute::Menu;return p;
}
template<class Native,class Ray> bool ExecutePick(const PickPlan& p,Native native,Ray ray){
    if(p.route==PickRoute::Native)return native();
    if(p.route==PickRoute::Miss)return false;
    ray(p.camera,p.x,p.y);return true;
}
}
