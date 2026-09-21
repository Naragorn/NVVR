#include "UiPick.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace nvvr;
using namespace nvvr::ui;
unsigned checks=0,nativeCalls=0,rayCalls=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
bool nativeResult=false;
bool __fastcall Native(void* camera,void*,unsigned x,unsigned y,void* origin,void* direction,void* target){
    ++nativeCalls;CHECK(camera==reinterpret_cast<void*>(0x123400));CHECK(x==17&&y==23);
    CHECK(origin==reinterpret_cast<void*>(0x100)&&direction==reinterpret_cast<void*>(0x200)&&target==reinterpret_cast<void*>(0x300));
    return nativeResult;
}
void __fastcall Ray(const void* camera,void*,float x,float y,void* origin,void* direction){
    ++rayCalls;const auto& c=*static_cast<const probe::Camera*>(camera);
    CHECK(c.frustum.right==1700&&c.world.position[0]==-850);CHECK(x==425&&y==720);
    CHECK(origin==reinterpret_cast<void*>(0x100)&&direction==reinterpret_cast<void*>(0x200));
}
int main(){
    probe::Camera c{};c.vtable=probe::CameraVtable;c.world.scale=1;
    c.world.rotation[2]=1;c.world.rotation[3]=1;c.world.rotation[7]=1;
    c.world.position[0]=-573.839f;c.world.position[2]=-480;
    c.frustum={0,1147.678f,960,0,1,100,1,{}};
    const PickLayout layout{0x123400,1700,960};
    for(bool enabled:{false,true})for(bool target:{false,true})
      for(unsigned mismatch=0;mismatch<8;++mismatch)for(bool outside:{false,true}){
        auto camera=c;auto l=layout;auto size=resolution::Size{4000,2250};unsigned address=layout.camera;
        if(mismatch==1)address+=4;if(mismatch==2)l.camera=address=0;
        if(mismatch==3)camera.vtable=0;if(mismatch==4)size.height=0;
        if(mismatch==5)l.width=0;if(mismatch==6)camera.frustum.ortho=0;
        if(mismatch==7)camera.world.rotation[0]=std::numeric_limits<float>::quiet_NaN();
        auto p=PlanPick(enabled,address,camera,l,size,outside?4001:1000,0,target);
        CHECK(p.route==(!enabled||target||mismatch?PickRoute::Native:outside?PickRoute::Miss:PickRoute::Menu));
      }
    for(auto size:{resolution::Size{4036,2279},resolution::Size{4000,2250},resolution::Size{256,512}})
      for(unsigned x:{0u,size.width/2,size.width,size.width+1,UINT32_MAX})
        for(unsigned y:{0u,size.height/2,size.height,size.height+1,UINT32_MAX}){
            auto p=PlanPick(true,layout.camera,c,layout,size,x,y,false);
            if(x>size.width||y>size.height){CHECK(p.route==PickRoute::Miss);continue;}
            CHECK(p.route==PickRoute::Menu);
            // Project the pick origin back through the drawing camera.
            const float worldX=p.camera.world.position[0]+p.x;
            const float worldZ=p.camera.world.position[2]+p.y;
            CHECK(std::fabs((worldX+850)/1700-double(x)/size.width)<0.000001);
            CHECK(std::fabs(1-(worldZ+480)/960-double(y)/size.height)<0.000001);
        }
    CHECK(std::fabs(c.world.position[0]+573.839f)<0.001f); // no live camera mutation
    auto plan=PlanPick(true,layout.camera,c,layout,{4000,2240},1000,560,false);
    for(auto route:{PickRoute::Native,PickRoute::Miss,PickRoute::Menu})for(bool result:{false,true}){
        plan.route=route;nativeResult=result;nativeCalls=rayCalls=0;
        unsigned short before=0,after=0;__asm fnstcw before
        const bool actual=TestPick(reinterpret_cast<void*>(0x123400),17,23,reinterpret_cast<void*>(0x100),
            reinterpret_cast<void*>(0x200),reinterpret_cast<void*>(0x300),
            reinterpret_cast<PickNative>(&Native),reinterpret_cast<PickRay>(&Ray),plan);
        __asm fnstcw after
        CHECK(before==after);CHECK(actual==(route==PickRoute::Menu||(route==PickRoute::Native&&result)));
        CHECK(nativeCalls==(route==PickRoute::Native?1u:0u));CHECK(rayCalls==(route==PickRoute::Menu?1u:0u));
    }
    std::printf("%u UI mouse mapping and native ABI checks passed\n",checks);
}
