#pragma once
#include "render/D3D9Types.h"
#include "render/D3D11Types.h"
namespace nvvr::presentation {
// Copy the fully composed screen AFTER engine drawing, into an opaque cinema
// texture. No render target, viewport or blend-state changes are needed.
inline bool CopyScreen(void* device,void* target){
    namespace d=obvr::render::d3d9;namespace com=obvr::render::d3d11;
    if(!target)return false;
    const auto get=d::Method<d::GetBackBufferFn>(device,d::kDeviceGetBackBuffer);
    const auto copy=d::Method<d::StretchRectFn>(device,d::kDeviceStretchRect);
    if(!get || !copy)return false;
    void* source=nullptr;
    const auto result=get(device,0,0,d::kBackBufferTypeMono,&source);
    if(com::Failed(result) || !source){com::Release(source);return false;}
    const bool ok=!com::Failed(copy(device,source,nullptr,target,nullptr,0));
    com::Release(source);return ok;
}
}
