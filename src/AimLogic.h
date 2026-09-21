#pragma once
#include "RotationLogic.h"
namespace nvvr::aim {
struct Launch {std::uint32_t words[16]{};};
inline float Float(const Launch& p,unsigned i){float v=0;std::memcpy(&v,&p.words[i],4);return v;}
inline bool Angles(bool enabled,probe::Address player,const Launch& args,const probe::Transform& gaze,std::array<float,2>& result){
    // Only the ordinary player's untargeted shot. Targeted/VATS paths keep
    // their own destination; actor identity is argument 1 at 005245BD.
    if(!enabled || player<0x10000 || args.words[1]!=player || !args.words[0] || args.words[10] || args.words[11])return false;
    if(!probe::PlausibleTransform(gaze))return false;
    const float spreadYaw=Float(args,13),spreadPitch=Float(args,14);
    if(!std::isfinite(spreadYaw)||!std::isfinite(spreadPitch)||std::fabs(spreadYaw)>3.141593f||std::fabs(spreadPitch)>3.141593f)return false;
    const float x=gaze.rotation[0],y=gaze.rotation[3],z=gaze.rotation[6];
    result={std::atan2(x,y)+spreadYaw,-std::atan2(z,std::sqrt(x*x+y*y))+spreadPitch};
    return std::isfinite(result[0])&&std::isfinite(result[1]);
}
enum class Result { Refused, Applied, Restored, RestoreFailed };
inline Result Apply(bool enabled,probe::Address player,const Launch& args,const probe::Transform& gaze,probe::Address address,void* context,rotation::WriteFn write){
    std::array<float,2> angles{};
    if(!Angles(enabled,player,args,gaze,angles)||!write||address<0x10000||address>UINT32_MAX-8)return Result::Refused;
    if(write(context,address,angles.data(),8))return Result::Applied;
    return write(context,address,args.words+7,8)?Result::Restored:Result::RestoreFailed;
}
}
