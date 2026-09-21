#include "MenuGeometry.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
using namespace nvvr;
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Memory {
    std::vector<unsigned char> bytes=std::vector<unsigned char>(4096);
    unsigned calls=0,fail=0;
    template<class T> void Put(unsigned offset,T value){std::memcpy(bytes.data()+offset,&value,sizeof(value));}
    static bool Copy(void* raw,probe::Address address,void* out,std::size_t size){
        auto& m=*static_cast<Memory*>(raw);if(++m.calls==m.fail)return false;
        if(address<0x20000||address-0x20000+size>m.bytes.size())return false;
        std::memcpy(out,m.bytes.data()+address-0x20000,size);return true;
    }
    probe::Reader Reader(){return {this,Copy};}
    Memory(){Put(0x9C,0x20200u);Put(0x214,0x20300u);Put(0x218,2u);
        Put(0x300,0x20400u);Put(0x304,0x20420u);
        Put(0x400,0xFB1u);Put(0x408,1700.240967f);Put(0x420,0xFB0u);Put(0x428,960.f);}
};
int main(){
    probe::Frustum original{0,1147.677734f,960,0,1,10000,1,{}},result{};
    CHECK(ui::MenuProjection(original,1700.240967f,960,result));
    CHECK(result.right==1700.240967f&&result.top==960&&result.nearPlane==1&&result.farPlane==10000);
    // Measured regression: right-edge buttons were outside the old camera.
    CHECK(1600>original.right&&1600<result.right);
    const auto fitted=ui::FitMenu(result.right,result.top,{4036,3376});
    CHECK(fitted.width==4036&&fitted.height==2279);
    CHECK(std::fabs(float(fitted.width)/fitted.height-result.right/result.top)<0.001f);
    for(unsigned axis=0;axis<2;++axis)for(float bad:{0.f,-1.f,16385.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        CHECK(!ui::MenuProjection(original,axis?1700.f:bad,axis?bad:960.f,result));
        CHECK(!ui::FitMenu(axis?1700.f:bad,axis?bad:960.f,{4036,3376}).width);
    }
    for(unsigned mask=1;mask<8;++mask){auto bad=original;if(mask&1)bad.ortho=0;if(mask&2)bad.left=1;if(mask&4)bad.bottom=1;
        CHECK(!ui::MenuProjection(bad,1700,960,result));}
    for(auto size:{resolution::Size{},resolution::Size{100,400},resolution::Size{400,100},resolution::Size{20000,400},resolution::Size{400,20000}})
        CHECK(!ui::FitMenu(1700,960,size).width);
    const auto portrait=ui::FitMenu(960,1700,{4036,3376});CHECK(portrait.height==3376&&portrait.width==1906);
    const auto equal=ui::FitMenu(1,1,{1000,1000});CHECK(equal.width==1000&&equal.height==1000);
    CHECK(!ui::FitMenu(0.000001f,16000,{400,400}).width);
    CHECK(!ui::FitMenu(16000,0.000001f,{400,400}).width);
    for(unsigned mask=0;mask<256;++mask){
        const bool active=!(mask&1);const unsigned x=(mask&2)?1:0,y=(mask&4)?1:0;
        const unsigned w=(mask&8)?4035:4036,h=(mask&16)?3375:3376;
        resolution::Size content{(mask&32)?0u:4036u,(mask&64)?0u:(mask&128)?3377u:2279u};
        CHECK(ui::FitViewport(active,x,y,w,h,{4036,3376},content)==(mask==0));
    }
    CHECK(!ui::FitViewport(true,0,0,4036,3376,{4036,3376},{4037,100}));
    for(unsigned fail=0;fail<=9;++fail){Memory m;m.fail=fail;float width=0,height=0;
        CHECK(ui::ReadMenuSize(m.Reader(),0x20000,width,height)==(fail==0));
        if(!fail)CHECK(width==1700.240967f&&height==960);}
    for(unsigned count:{0u,1u,257u}){Memory m;m.Put(0x218,count);float w=0,h=0;CHECK(!ui::ReadMenuSize(m.Reader(),0x20000,w,h));}
    for(unsigned id:{0xFA1u,0xFB0u}){Memory m;m.Put(0x400,id);float w=0,h=0;CHECK(!ui::ReadMenuSize(m.Reader(),0x20000,w,h));}
    {Memory m;m.Put(0x420,0xFB1u);float w=0,h=0;CHECK(!ui::ReadMenuSize(m.Reader(),0x20000,w,h));}
    // Measured main-menu camera: widening only the frustum loses 276.218 units
    // on the left and leaves the same amount empty on the right.
    probe::Camera camera{},adjusted{};
    camera.frustum=original;camera.world={{0,0,1,1,0,0,0,1,0},{-573.838867f,0,-480},1};
    CHECK(ui::MenuCamera(camera,1700.114f,960,adjusted));
    CHECK(std::fabs(adjusted.world.position[0]+850.057f)<0.001f);
    CHECK(adjusted.world.position[2]==-480);
    for(float edge:{-850.057f,850.057f}){
        const float screen=(edge-adjusted.world.position[0])/adjusted.frustum.right;
        CHECK(std::fabs(screen-(edge<0?0.f:1.f))<0.00001f);
    }
    // Both dimensions can grow, shrink or stay; test arbitrary camera bases.
    for(auto basis:{probe::Transform{{1,0,0,0,1,0,0,0,1},{23,42,51},1},camera.world})
      for(float width:{800.f,1147.677734f,1700.114f})
        for(float height:{600.f,960.f,1200.f}){
            auto c=camera;c.world=basis;CHECK(ui::MenuCamera(c,width,height,adjusted));
            for(unsigned row=0;row<3;++row){
                const float oldCentre=c.world.position[row]+basis.rotation[row*3+2]*c.frustum.right/2+basis.rotation[row*3+1]*c.frustum.top/2;
                const float newCentre=adjusted.world.position[row]+basis.rotation[row*3+2]*width/2+basis.rotation[row*3+1]*height/2;
                CHECK(std::fabs(oldCentre-newCentre)<0.001f);
            }
        }
    for(unsigned mode=0;mode<7;++mode){
        auto c=camera;const float nan=std::numeric_limits<float>::quiet_NaN();
        if(mode==0)c.frustum.ortho=0;
        if(mode==1)c.frustum.right=nan;
        if(mode==2)c.frustum.top=nan;
        if(mode==3)c.frustum.right=0;
        if(mode==4)c.frustum.top=-1;
        if(mode==5)c.world.rotation[0]=2;
        if(mode==6)c.world.position[0]=nan;
        CHECK(!ui::MenuCamera(c,1700,960,adjusted));
    }
        {
        auto huge=camera;huge.world.position[0]=(std::numeric_limits<float>::max)();
        huge.frustum.right=(std::numeric_limits<float>::max)();
        CHECK(!ui::MenuCamera(huge,1700,960,adjusted));
    }
    CHECK(!ui::MenuCamera(camera,0,960,adjusted));
    CHECK(!ui::MenuCamera(camera,1700,0,adjusted));
    CHECK(ui::MenuCamera(camera,1700.114f,960,adjusted));
    // Successful/partial apply and restore failures, with actual camera memory.
    for(unsigned failure=0;failure<4;++failure){
        auto live=camera;live.local.position[0]=123;
        unsigned writes=0,updates=0;
        auto writer=[&](probe::Address address,const void* data,std::size_t size){
            CHECK(address==0x20068&&size==ui::MenuCameraBytes);
            const bool fail=(failure&(1u<<writes++))!=0;
            std::memcpy(&live.world,data,fail?size/2:size);
            return !fail;
        };
        CHECK(ui::ApplyMenuCamera(0x20000,adjusted,writer,[&](probe::Address address){CHECK(address==0x20000);++updates;})==!(failure&1));
        CHECK(updates==((failure&1)?0u:1u));
        CHECK(ui::RestoreMenuCamera(0x20000,camera,writer)==!(failure&2));
        CHECK(writes==2&&live.local.position[0]==123);
        if(!(failure&2))CHECK(std::memcmp(&live.world,&camera.world,ui::MenuCameraBytes)==0);
    }
    std::printf("%u menu geometry checks passed\n",checks);return 0;
}