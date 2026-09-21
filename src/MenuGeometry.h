#pragma once
#include "EngineProbe.h"
#include "ResolutionLogic.h"
namespace nvvr::ui {
inline resolution::Size captureExtent{};
inline bool MenuProjection(const probe::Frustum& original,float width,float height,probe::Frustum& result){
    if(!original.ortho||original.left!=0||original.bottom!=0||
       !std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0||width>16384||height>16384)return false;
    result=original;result.right=width;result.top=height;return true;
}
inline bool MenuCamera(const probe::Camera& original,float width,float height,probe::Camera& result){
    probe::Frustum frustum{};
    if(!MenuProjection(original.frustum,width,height,frustum)||
       !std::isfinite(original.frustum.right)||!std::isfinite(original.frustum.top)||
       original.frustum.right<=0||original.frustum.top<=0||
       !probe::PlausibleTransform(original.world))return false;
    result=original;result.frustum=frustum;
    // UI geometry is centred about the origin. Preserve the camera's centre
    // when its orthographic rectangle changes, along camera right and up.
    const float right=(original.frustum.right-width)*0.5f;
    const float up=(original.frustum.top-height)*0.5f;
    for(unsigned row=0;row<3;++row)
        result.world.position[row]+=original.world.rotation[row*3+2]*right+
                                    original.world.rotation[row*3+1]*up;
    return probe::PlausibleTransform(result.world);
}
inline resolution::Size FitMenu(float width,float height,resolution::Size target){
    if(!resolution::Valid(target)||!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0||width>16384||height>16384)return {};
    const double aspect=double(width)/height;
    resolution::Size result=target;
    if(double(target.width)/target.height>aspect)result.width=static_cast<unsigned>(target.height*aspect+0.5);
    else result.height=static_cast<unsigned>(target.width/aspect+0.5);
    return result.width&&result.height?result:resolution::Size{};
}
inline bool ReadMenuSize(const probe::Reader& reader,probe::Address object,float& width,float& height){
    probe::Address root=0,values=0;unsigned count=0;bool haveWidth=false,haveHeight=false;
    if(!reader.Value(object,0x9C,root)||!reader.Value(root,0x14,values)||!reader.Value(root,0x18,count)||count>256)return false;
    for(unsigned i=0;i<count;++i){probe::Address value=0;unsigned id=0;float number=0;
        if(!reader.Value(values,i*4,value)||!reader.Value(value,0,id)||!reader.Value(value,8,number))return false;
        if(id==0xFB1){width=number;haveWidth=true;}if(id==0xFB0){height=number;haveHeight=true;}
    }
    return haveWidth&&haveHeight;
}
inline bool FitViewport(bool capturing,unsigned x,unsigned y,unsigned width,unsigned height,resolution::Size target,resolution::Size content){
    return capturing&&x==0&&y==0&&width==target.width&&height==target.height&&
        content.width>0&&content.height>0&&content.width<=width&&content.height<=height;
}
inline constexpr std::size_t MenuCameraBytes=sizeof(probe::Transform)+64+sizeof(probe::Frustum);
static_assert(MenuCameraBytes==144);
static_assert(offsetof(probe::Camera,frustum)+sizeof(probe::Frustum)-offsetof(probe::Camera,world)==MenuCameraBytes);
template<class Write,class Update> bool ApplyMenuCamera(probe::Address camera,const probe::Camera& target,Write write,Update update){
    if(!write(camera+0x68,&target.world,MenuCameraBytes))return false;
    update(camera);return true;
}
template<class Write> bool RestoreMenuCamera(probe::Address camera,const probe::Camera& saved,Write write){
    return write(camera+0x68,&saved.world,MenuCameraBytes);
}
}