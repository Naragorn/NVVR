#include <initializer_list>
#include "AnchorLogic.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace nvvr::ui;
using Matrix=obvr::vr::openvr::HmdMatrix34;
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
Matrix Identity(){Matrix m{};m.m[0][0]=m.m[1][1]=m.m[2][2]=1;return m;}
int main(){
    for(bool valid:{false,true})for(bool pose:{false,true})
      for(float distance:{-1.f,0.f,4.f,4.01f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        const auto actual=PlanAnchor(valid,pose,distance);
        const auto expected=!pose?(valid?AnchorAction::Keep:AnchorAction::Wait):
            (!std::isfinite(distance)||distance<0)?AnchorAction::Wait:
            valid&&distance<=4?AnchorAction::Keep:AnchorAction::Place;
        CHECK(actual==expected);
      }
    for(bool valid:{false,true})for(bool pose:{false,true})for(float x:{0.f,2.f,3.f})
      for(int result:{-1,0,10}){
        Matrix saved=Identity(),current=Identity(),submitted{};current.m[0][3]=x;
        unsigned calls=0;bool anchored=valid;
        const bool ok=AnchorRoom(anchored,saved,pose?&current:nullptr,1.5f,
            [&](const Matrix& m){++calls;submitted=m;return result;});
        const bool place=pose&&(!valid||x>2);
        CHECK(calls==(place?1u:0u));CHECK(ok==(place?result==0:valid));
        CHECK(anchored==(valid||(place&&result==0)));
        CHECK(saved.m[0][3]==(place&&result==0?x:0));
        if(place){CHECK(submitted.m[0][3]==x&&submitted.m[2][3]==-1.5f);}
      }
    bool valid=false;auto saved=Identity(),head=Identity();unsigned calls=0;
    auto set=[&](const Matrix&){return ++calls<3?5:0;};
    CHECK(!AnchorRoom(valid,saved,&head,1.5f,set)&&!valid);
    CHECK(!AnchorRoom(valid,saved,&head,1.5f,set)&&!valid);
    CHECK(AnchorRoom(valid,saved,&head,1.5f,set)&&valid&&calls==3);
    // Turning the head leaves the absolute transform untouched.
    head.m[0][0]=0;head.m[0][2]=1;head.m[2][0]=-1;head.m[2][2]=0;
    CHECK(AnchorRoom(valid,saved,&head,1.5f,set)&&calls==3&&saved.m[0][0]==1);
    valid=false; // Delete/recenter explicitly requests a new heading.
    CHECK(AnchorRoom(valid,saved,&head,1.5f,set)&&calls==4&&saved.m[0][2]==1);
    CHECK(AnchorRoom(valid,saved,nullptr,1.5f,set)&&calls==4);
    std::printf("%u spatial anchor flow checks passed\n",checks);
}

namespace obvr::log { void Write(const char*,...){} }
