#pragma once
#include "render/GameFrame.h"
#include "StereoPlacement.h"
#include "render/D3D11Types.h"
namespace nvvr::stereo {
namespace gpu=obvr::render;
template<class WaitFn> bool WaitUnderQueue(void* device,WaitFn wait){
    gpu::InteropBracket poses;
    if(!poses.Begin(device))return false;
    return wait()==0;
}
// This pipeline produces equal-sized, separately owned eye images. Reject a
// partial resize or accidental alias before changing layouts or submitting.
inline bool MatchingImages(const gpu::BackBufferImage& left,const gpu::BackBufferImage& right){
    return left.image!=right.image && left.width==right.width &&
           left.height==right.height && left.format==right.format &&
           left.sampleCount==right.sampleCount;
}
class FramePair {
    gpu::FrameResolve copies[2],resolved;
    void* interop[2]{};
    void* device=nullptr;
public:
    ~FramePair(){Reset();}
    void ReleaseReferences(){for(auto& p:interop)gpu::d3d11::Release(p);}
    void Reset(){ReleaseReferences();for(auto& c:copies)c.Destroy();resolved.Destroy();device=nullptr;}
    bool Begin(void* next){

        if(next!=device){Reset();device=next;}
        ReleaseReferences();
        return gpu::IdentifyDevice(device)==gpu::DeviceKind::Dxvk;
    }
    bool Copy(unsigned eye,bool black=false,const Placement* placement=nullptr){
        if(eye>=2)return false;
        gpu::d3d11::Release(interop[eye]);
        void* back=nullptr;
        const auto get=gpu::d3d9::Method<gpu::d3d9::GetBackBufferFn>(device,gpu::d3d9::kDeviceGetBackBuffer);
        if(!get || gpu::d3d11::Failed(get(device,0,0,gpu::d3d9::kBackBufferTypeMono,&back)) || !back)return false;
        gpu::d3d9::SurfaceDesc desc{};
        const auto describe=gpu::d3d9::Method<gpu::d3d9::GetDescFn>(back,gpu::d3d9::kSurfaceGetDesc);
        void* surface=nullptr;
        if(describe && !gpu::d3d11::Failed(describe(back,&desc))){
            if(!placement)surface=copies[eye].ResolveFrom(device,back,desc);
            else{
                gpu::d3d9::Rect source{},dest{};
                if(Rectangles(*placement,desc.width,desc.height,source,dest)){
                    // Resolve MSAA whole-surface before cropping/scaling. A
                    // cropped multisample resolve is not used as a blit.
                    void* single=resolved.ResolveFrom(device,back,desc);
                    if(single){desc.multiSampleType=0;desc.multiSampleQuality=0;
                        surface=copies[eye].ResolveFrom(device,single,desc,&source,&dest,true);
                        gpu::d3d11::Release(single);}
                }
            }
        }
        gpu::d3d11::Release(back);
        if(!surface)return false;
        if(black){
            const auto fill=gpu::d3d9::Method<gpu::d3d9::ColorFillFn>(device,gpu::d3d9::kDeviceColorFill);
            if(!fill||gpu::d3d11::Failed(fill(device,surface,nullptr,0xFF000000u))){gpu::d3d11::Release(surface);return false;}
        }
        auto* unknown=static_cast<gpu::d3d11::Unknown*>(surface);
        const bool ok=unknown->vtbl && unknown->vtbl->QueryInterface &&
            !gpu::d3d11::Failed(unknown->vtbl->QueryInterface(unknown,&gpu::kIID_D3D9VkInteropTexture,&interop[eye])) && interop[eye];
        gpu::d3d11::Release(surface);return ok;
    }
    template<class SubmitFn> bool Submit(SubmitFn submit){
        gpu::VulkanContext context{};gpu::BackBufferImage images[2]{};
        if(!gpu::GetVulkanContext(device,context))return false;
        for(unsigned i=0;i<2;++i)if(!gpu::ReadImageInfo(interop[i],images[i])||!gpu::IsSubmittableImage(images[i]))return false;
        if(!MatchingImages(images[0],images[1]))return false;
        gpu::InteropBracket bracket;
        if(!bracket.Begin(device))return false;
        for(unsigned i=0;i<2;++i)if(!bracket.ToTransferSrc(interop[i],images[i].layout))return false;
        gpu::dxvk::VRVulkanTextureData textures[2]{};
        for(unsigned i=0;i<2;++i)gpu::DescribeForOpenVR(images[i],context,textures[i]);
        return submit(&textures[0],&textures[1]);
    }
};

}
