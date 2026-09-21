#pragma once
#include "StereoLogic.h"
#include <algorithm>
#include <limits>
#include "render/EyeGeometry.h"
#include "render/D3D9Types.h"
namespace nvvr::stereo {
using Placement=obvr::render::PicturePlacement;
inline bool PreparePair(const Eye (&raw)[2],Eye (&render)[2],Placement (&placement)[2]){
    float w=0,h=0;
    for(const auto& e:raw){const auto& p=e.projection;
        for(float v:p)if(!std::isfinite(v))return false;
        if(!(p[0]<0&&p[1]>0&&p[2]<0&&p[3]>0))return false;
        w=(std::max)(w,(std::max)(-p[0],p[1]));h=(std::max)(h,(std::max)(-p[2],p[3]));
    }
    if(w>(std::numeric_limits<float>::max)()/2 || h>(std::numeric_limits<float>::max)()/2)return false;
    for(unsigned i=0;i<2;++i){
        render[i]=raw[i];render[i].projection={-w,w,-h,h};
        const auto& p=raw[i].projection;
        placement[i]=obvr::render::PlacePictureFromTangents({p[0],p[1],p[2],p[3]},w,h);
    }
    return true;
}
inline bool Rectangles(const Placement& p,unsigned width,unsigned height,obvr::render::d3d9::Rect& source,obvr::render::d3d9::Rect& dest){
    if(!width||!height||width>16384||height>16384)return false;
    const float values[]={p.sourceUMin,p.sourceVMin,p.sourceUMax,p.sourceVMax,p.uMin,p.vMin,p.uMax,p.vMax};
    for(float v:values)if(!std::isfinite(v)||v<0||v>1)return false;
    auto edge=[](float value,unsigned size){return static_cast<SInt32>(value*size+0.5f);};
    source={edge(values[0],width),edge(values[1],height),edge(values[2],width),edge(values[3],height)};
    dest={edge(values[4],width),edge(values[5],height),edge(values[6],width),edge(values[7],height)};
    return source.left<source.right&&source.top<source.bottom&&dest.left<dest.right&&dest.top<dest.bottom;
}
}
