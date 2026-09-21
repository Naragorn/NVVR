#pragma once
#include "HookLogic.h"
namespace nvvr::rotation {
inline constexpr float TestAngle=0.1745329252f; // 10 degrees, radians
struct Toggle {
    bool enabled=false,down=false;
    void Step(bool pressed,bool focused,bool blocked) {
        if(!focused || blocked) enabled=false;
        else if(pressed&&!down) enabled=!enabled;
        down=pressed;
    }
    void Reset(){enabled=false;}
};
inline bool Yaw(const probe::Transform& base,float angle,std::array<float,9>& result) {
    if(!std::isfinite(angle) || std::fabs(angle)>0.25f || !probe::PlausibleTransform(base)) return false;
    const float c=std::cos(angle),s=std::sin(angle);
    // Columns from 00A70BA0: forward, up, right. Rotate around local up.
    // R' = R * Ry(angle); original position/scale and local transform are untouched.
    for(unsigned row=0;row<3;++row){
        result[row*3]=base.rotation[row*3]*c-base.rotation[row*3+2]*s;
        result[row*3+1]=base.rotation[row*3+1];
        result[row*3+2]=base.rotation[row*3]*s+base.rotation[row*3+2]*c;
    }
    return true;
}
enum class Result { Disabled, OtherPath, NotWorld, Unreadable, Collision, Invalid, NoWriter, Applied, WriteFailedRestored, RestoreFailed };
inline const char* Name(Result r) {
    constexpr const char* names[]={"disabled","other-path","not-world","unreadable","collision","invalid","no-writer","applied","write-failed-restored","restore-failed"};
    return names[static_cast<unsigned>(r)];
}
using WriteFn=bool(*)(void*,probe::Address,const void*,std::size_t);
inline bool Compose(const probe::Transform& base,const std::array<float,9>& delta,std::array<float,9>& result){
    probe::Transform d{};std::memcpy(d.rotation,delta.data(),sizeof(d.rotation));d.scale=1;
    if(!probe::PlausibleTransform(base)||!probe::PlausibleTransform(d))return false;
    for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col){float sum=0;for(unsigned k=0;k<3;++k)sum+=base.rotation[row*3+k]*delta[k*3+col];result[row*3+col]=sum;}
    return true;
}
// Initial scale inherited from OBVR 0.1.3 HeadTracker.h. NV world scale has
// not yet been calibrated against a measured in-game distance.
inline constexpr float UnitsPerMetre=69.99125f;
inline bool Translate(const probe::Transform& base,const std::array<float,3>& metres,std::array<float,3>& result,float units=UnitsPerMetre){
    if(!probe::PlausibleTransform(base)||!std::isfinite(units)||units<=0)return false;
    for(float v:metres)if(!std::isfinite(v))return false;
    for(unsigned row=0;row<3;++row){
        float distance=0;
        for(unsigned k=0;k<3;++k)distance+=base.rotation[row*3+k]*metres[k];
        result[row]=base.position[row]+distance*units;
        if(!std::isfinite(result[row]))return false;
    }
    return true;
}
inline Result Apply(bool enabled,unsigned site,probe::Address camera,const hook::Sample& snapshot,const probe::Reader& reader,void* context,WriteFn write,const std::array<float,9>* head=nullptr,const std::array<float,3>* offset=nullptr) {
    if(!enabled)return Result::Disabled;
    if(site!=0)return Result::OtherPath;
    if(snapshot.kind!=hook::Kind::World)return Result::NotWorld;
    if(!snapshot.before)return Result::Unreadable;
    probe::Address collision=0;
    if(!reader.Value(camera,0x1C,collision))return Result::Unreadable;
    if(collision)return Result::Collision;
    std::array<float,9> rotated{};
    if(!(head?Compose(snapshot.a.world,*head,rotated):Yaw(snapshot.a.world,TestAngle,rotated)))return Result::Invalid;
    std::array<float,12> changed{},original{};
    std::memcpy(changed.data(),rotated.data(),36);
    std::memcpy(original.data(),snapshot.a.world.rotation,36);
    std::memcpy(original.data()+9,snapshot.a.world.position,12);
    if(offset){
        std::array<float,3> translated{};
        if(!Translate(snapshot.a.world,*offset,translated))return Result::Invalid;
        std::memcpy(changed.data()+9,translated.data(),12);
    }
    if(!write)return Result::NoWriter;
    const auto address=camera+0x68;
    const std::size_t size=offset?48:36;
    if(write(context,address,changed.data(),size))return Result::Applied;
    // Restore rotation AND position after a possibly partial copy.
    return write(context,address,original.data(),size) ? Result::WriteFailedRestored : Result::RestoreFailed;
}
}
