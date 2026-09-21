#include "AimLogic.h"
#include "UiLogic.h"
#include "SessionLogic.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <limits>
using namespace nvvr;
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
void Put(aim::Launch& p,unsigned i,float v){std::memcpy(&p.words[i],&v,4);}
bool Near(float a,float b){return std::fabs(a-b)<0.00001f;}
struct UiOps {
    unsigned bits;std::string trace;
    bool Save(){trace+='S';return bits&2;}
    bool Begin(){trace+='B';return bits&4;}
    void Render(){trace+='D';}
    void End(){trace+='E';}
    bool Restore(){trace+='R';return bits&8;}
};
struct AimWriter {
    unsigned mask=0,calls=0;float output[2]{};
    static bool Write(void* context,probe::Address address,const void* bytes,std::size_t size){auto& self=*static_cast<AimWriter*>(context);CHECK(address==0x30000&&size==8);std::memcpy(self.output,bytes,size);return !(self.mask&(1u<<self.calls++));}
};
int main(){
    for(unsigned type=0;type<256;++type){
        const auto expected=type==1||type==7?SessionAction::Stop:type==2||type==6||type==14?SessionAction::ResetWorld:SessionAction::None;
        CHECK(OnLifecycle(type)==expected);
    }
    probe::Transform gaze{{1,0,0,0,1,0,0,0,1},{0,0,0},1};
    aim::Launch args{};args.words[0]=0x30000;args.words[1]=0x20000;
    std::array<float,2> angles{};
    CHECK(aim::Angles(true,0x20000,args,gaze,angles));CHECK(Near(angles[0],1.5707963268f)&&Near(angles[1],0));
    Put(args,13,0.1f);Put(args,14,-0.2f);
    CHECK(aim::Angles(true,0x20000,args,gaze,angles));CHECK(Near(angles[0],1.6707963268f)&&Near(angles[1],-0.2f));
    auto up=gaze;const float pitched[9]={0,1,0,0,0,1,1,0,0};std::memcpy(up.rotation,pitched,36);
    CHECK(aim::Angles(true,0x20000,args,up,angles));CHECK(Near(angles[1],-1.7707963268f));
    for(unsigned bits=0;bits<64;++bits){auto p=args;const bool enabled=bits&1;auto player=bits&2?0x20000u:0u;
        p.words[1]=bits&4?0x20000:0x40000;p.words[0]=bits&8?0x30000:0;p.words[10]=bits&16?0:1;p.words[11]=bits&32?0:1;
        CHECK(aim::Angles(enabled,player,p,gaze,angles)==(bits==63));}
    auto invalid=gaze;invalid.scale=0;CHECK(!aim::Angles(true,0x20000,args,invalid,angles));
    for(unsigned mask=0;mask<4;++mask){AimWriter w{mask};const auto r=aim::Apply(true,0x20000,args,gaze,0x30000,&w,AimWriter::Write);
        CHECK(r==(!(mask&1)?aim::Result::Applied:mask&2?aim::Result::RestoreFailed:aim::Result::Restored));
        if(mask&1)CHECK(std::memcmp(w.output,args.words+7,8)==0);}
    AimWriter writer{};CHECK(aim::Apply(false,0x20000,args,gaze,0x30000,&writer,AimWriter::Write)==aim::Result::Refused);
    CHECK(aim::Apply(true,0x20000,args,gaze,0x30000,&writer,nullptr)==aim::Result::Refused);
    for(auto address:{0u,UINT32_MAX})CHECK(aim::Apply(true,0x20000,args,gaze,address,&writer,AimWriter::Write)==aim::Result::Refused);
    for(unsigned field:{13u,14u})for(float value:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),3.15f,-3.15f}){auto p=args;Put(p,field,value);CHECK(!aim::Angles(true,0x20000,p,gaze,angles));}
    for(unsigned state=0;state<256;++state)for(unsigned value:{0u,1u,7u,0xFFFFFFFFu}){
        ui::RequestedStates requested;requested.values.fill(42);requested.Remember(state,value);
        for(unsigned i=0;i<ui::ChangedStates.size();++i)CHECK(requested.values[i]==(ui::ChangedStates[i]==state?value:42));
        CHECK(ui::RenderState(false,state,value)==value);
        const auto mapped=ui::RenderState(true,state,value);
        if(state==7)CHECK(mapped==0);
        else if(state==168)CHECK(mapped==(value|15u));
        else if(state==206||state==209)CHECK(mapped==1);
        else if(state==207)CHECK(mapped==2);
        else if(state==208)CHECK(mapped==6);
        else CHECK(mapped==value);
    }
    for(unsigned bits=0;bits<16;++bits){UiOps ops{bits};const bool result=ui::Capture(bool(bits&1),ops);
        const std::string expected=!(bits&1)?"D":!(bits&2)?"SD":!(bits&4)?"SBRD":"SBDER";
        CHECK(ops.trace==expected);CHECK(result==(bits==15));}
    std::printf("%u aiming/UI checks passed\n",checks);return 0;
}
