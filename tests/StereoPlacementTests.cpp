#include "StereoPlacement.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
namespace obvr::log {void Write(const char*,...) {}}
using namespace nvvr::stereo;
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
void Near(float a,float b){CHECK(std::fabs(a-b)<0.00001f);}
int main(){
    // Independently map rays through the common game frustum and the final
    // texture crop. Expected coordinates come directly from each eye's edges.
    for(float l:{0.4f,1.0f,1.7f})for(float r:{0.5f,1.1f,1.8f})
    for(float t:{0.3f,0.9f,1.6f})for(float b:{0.6f,1.2f,1.9f}){
        Eye raw[2]{},render[2]{};Placement p[2]{};
        raw[0].projection={-l,r,-t,b};raw[1].projection={-r,l,-b,t};
        raw[0].eyeMetres={0,0,-0.032f};raw[1].eyeMetres={0,0,0.032f};
        CHECK(PreparePair(raw,render,p));CHECK(render[0].projection==render[1].projection);
        for(unsigned eye=0;eye<2;++eye){
            CHECK(raw[eye].eyeMetres==render[eye].eyeMetres);
            const auto& e=raw[eye].projection;const auto& c=render[eye].projection;const auto& q=p[eye];
            Near(c[0],-c[1]);Near(c[2],-c[3]);
            for(float f:{0.0f,0.25f,0.5f,0.75f,1.0f}){
                const float x=e[0]+f*(e[1]-e[0]),y=e[3]-f*(e[3]-e[2]);
                const float u=(x-c[0])/(c[1]-c[0]),v=(c[3]-y)/(c[3]-c[2]);
                Near(q.uMin+(u-q.sourceUMin)/(q.sourceUMax-q.sourceUMin)*(q.uMax-q.uMin),f);
                Near(q.vMin+(v-q.sourceVMin)/(q.sourceVMax-q.sourceVMin)*(q.vMax-q.vMin),f);
            }
            obvr::render::d3d9::Rect source{},dest{};
            CHECK(Rectangles(q,4036,3376,source,dest));
        }
    }
    Eye raw[2]{},render[2]{};Placement p[2]{};
    raw[0].projection=raw[1].projection={-1,1,-1,1};
    CHECK(PreparePair(raw,render,p));
    for(unsigned eye=0;eye<2;++eye)for(unsigned component=0;component<4;++component){
        const float original=raw[eye].projection[component];
        for(float bad:{0.0f,-original,std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),std::copysign(std::numeric_limits<float>::max(),original)}){
            raw[eye].projection[component]=bad;CHECK(!PreparePair(raw,render,p));
        }
        raw[eye].projection[component]=original;
    }
    CHECK(PreparePair(raw,render,p));
    obvr::render::d3d9::Rect source{},dest{};
    for(unsigned w:{0u,1u,16384u,16385u})for(unsigned h:{0u,1u,16384u,16385u})
        CHECK(Rectangles(p[0],w,h,source,dest)==(w>0&&w<=16384&&h>0&&h<=16384));
    using Member=float Placement::*;
    const Member fields[]={&Placement::sourceUMin,&Placement::sourceVMin,&Placement::sourceUMax,&Placement::sourceVMax,&Placement::uMin,&Placement::vMin,&Placement::uMax,&Placement::vMax};
    for(auto field:fields)for(float bad:{-0.1f,1.1f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        auto q=p[0];q.*field=bad;CHECK(!Rectangles(q,800,600,source,dest));
    }
    for(auto field:{&Placement::sourceUMax,&Placement::sourceVMax,&Placement::uMax,&Placement::vMax}){
        auto q=p[0];q.*field=0;CHECK(!Rectangles(q,800,600,source,dest));
        q.*field=0.00001f;CHECK(!Rectangles(q,800,600,source,dest));
    }
    std::printf("%u stereo placement checks passed\n",checks);
}
