#include "UiSize.h"
#include "UiSizeLogic.h"
#include <cstdio>
#include <cstdlib>
using namespace nvvr;
using namespace nvvr::ui;
unsigned checks=0,calls=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
unsigned __cdecl Original(){++calls;return 1176;}
int main(){
    auto fitted=SourceSize({4036,3376},{1176,664});
    CHECK(fitted.width==4036&&fitted.height==2279);
    auto portrait=SourceSize({4036,3376},{664,1176});
    CHECK(portrait.width==1906&&portrait.height==3376);
    for(unsigned fw:{0u,255u,256u,4036u,16384u,16385u,UINT32_MAX})
      for(unsigned fh:{0u,255u,256u,3376u,16384u,16385u,UINT32_MAX})
        for(auto window:{resolution::Size{},resolution::Size{1176,664},resolution::Size{664,1176},resolution::Size{256,16384},resolution::Size{16384,256}}){
            auto value=SourceSize({fw,fh},window);
            if(value.width){
                CHECK(resolution::Valid(value)&&value.width<=fw&&value.height<=fh);
                CHECK(value.width==fw||value.height==fh);
                // Independent aspect error bound of one rounded pixel.
                const auto difference=static_cast<long long>(value.width)*window.height-static_cast<long long>(value.height)*window.width;
                CHECK(std::llabs(difference)<=static_cast<long long>((std::max)(window.width,window.height)));
            }else CHECK(value.height==0);
        }
    for(bool ready:{false,true})for(bool width:{false,true})
      for(auto source:{resolution::Size{},resolution::Size{4036,2279},resolution::Size{255,2279},resolution::Size{4036,16385}}){
        sourceSize=source;
        unsigned short before=0,after=0;__asm fnstcw before
        const unsigned previous=calls,result=TestSizeGetter(width,ready,Original);
        __asm fnstcw after
        CHECK(calls==previous+1&&before==after);
        CHECK(result==(!ready||!resolution::Valid(source)?1176u:width?source.width:source.height));
      }
    constexpr unsigned n=sizeof(SizeSites)/sizeof(SizeSites[0]);
    // Every failed read or differing CALL byte refuses before any mutation.
    for(unsigned failure=0;failure<n;++failure)for(unsigned byte=0;byte<=5;++byte){
        unsigned reads=0,patches=0;
        const bool ok=InstallSizeSites([&](probe::Address address,std::array<unsigned char,5>& data){
            const unsigned index=reads++;CHECK(address==SizeSites[index].address);
            data=hook::Call(address,SizeSites[index].original);
            if(index==failure){if(byte==5)return false;data[byte]^=1;}return true;
        },[&](SizeSite){++patches;return hook::PatchResult{};});
        CHECK(!ok&&patches==0&&reads==failure+1);
    }
    // All 2048 combinations of installed/missing hooks; no asymmetric activation.
    for(unsigned mask=0;mask<(1u<<n);++mask){
        unsigned reads=0,patches=0;
        const bool ok=InstallSizeSites([&](probe::Address address,std::array<unsigned char,5>& data){
            data=hook::Call(address,SizeSites[reads++].original);return true;
        },[&](SizeSite site){
            CHECK(site.address==SizeSites[patches].address);
            hook::PatchResult r{};r.installed=(mask&(1u<<patches++))!=0;return r;
        });
        CHECK(ok==(mask==((1u<<n)-1))&&reads==n&&patches==n);
    }
    // A reported protection/flush/rollback failure also disables all size overrides.
    for(unsigned failure=0;failure<n;++failure)for(unsigned flags=0;flags<16;++flags){
        unsigned reads=0,patches=0;
        const bool ok=InstallSizeSites([&](probe::Address address,std::array<unsigned char,5>& data){
            data=hook::Call(address,SizeSites[reads++].original);return true;
        },[&](SizeSite){
            hook::PatchResult r{};r.installed=true;
            if(patches++==failure){r.installed=!(flags&1);r.restoreFailed=(flags&2)!=0;r.flushFailed=(flags&4)!=0;r.rollbackFlushFailed=(flags&8)!=0;}
            return r;
        });
        CHECK(ok==(flags==0)&&patches==n);
    }
    std::printf("%u UI size source/native checks passed\n",checks);return 0;
}
