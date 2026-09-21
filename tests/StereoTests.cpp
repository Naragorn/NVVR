#include "StereoLogic.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
using namespace nvvr;
using namespace nvvr::stereo;
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
bool Near(float a,float b){return std::fabs(a-b)<0.0001f;}
probe::Camera Camera(){probe::Camera c{};c.world={{1,0,0,0,1,0,0,0,1},{1,2,3},1};c.frustum={-1,1,1,-1,5,1000,0,{}};return c;}
Eye Params(){return {{1,0,0,0,1,0,0,0,1},{0,0,0},{0,0,-0.032f},{-1.1f,0.9f,-1.2f,1.0f}};}
struct Writer {
    unsigned mask=0,calls=0;CameraBlock actual{};
    static bool Write(void* raw,probe::Address address,const void* bytes,std::size_t size){auto& w=*static_cast<Writer*>(raw);CHECK(address==0x20068&&size==144);std::memcpy(&w.actual,bytes,size);return !(w.mask&(1u<<w.calls++));}
};
struct Ops {
    unsigned bits,index=0;std::string trace;
    bool Begin(){trace+='B';return bits&2;}
    bool SetEye(unsigned eye){index=eye;trace+=eye?'R':'L';return bits&(eye?8:4);}
    void Render(){trace+='D';}
    bool Copy(unsigned eye){trace+='C';return bits&(eye?32:16);}
    bool Restore(){trace+='U';return bits&(index?128:64);}
    bool Submit(){trace+='S';return bits&256;}
};
int main(){
    // Complete decision table: fresh/rebuild/unknown sites, all camera kinds,
    // read success/failure and active/inactive stereo.
    for(unsigned site:{0u,1u,2u,UINT32_MAX})
        for(auto kind:{hook::Kind::MissingWorld,hook::Kind::Other,hook::Kind::World})
            for(bool readable:{false,true})for(bool active:{false,true}){
                const bool fresh=site==0&&kind==hook::Kind::World&&readable;
                CHECK(RouteCamera(site,kind,readable,active)==
                    (!fresh?CameraAction::Preserve:active?CameraAction::ApplyEye:CameraAction::CaptureBaseline));
            }
    // A matrix rebuild must preserve an already composed eye transform:
    // applying this head turn twice changes the view from 90 to 180 degrees.
    {
        auto baseline=Camera();auto head=Params();head.head={0,0,1,0,1,0,-1,0,0};
        CameraBlock once{},twice{};CHECK(Build(baseline,head,once));
        auto already=baseline;already.world=once.world;CHECK(Build(already,head,twice));
        CHECK(!Near(once.world.rotation[0],twice.world.rotation[0]));
        CHECK(RouteCamera(1,hook::Kind::World,true,true)==CameraAction::Preserve);
        CHECK(RouteCamera(1,hook::Kind::World,true,false)==CameraAction::Preserve);
    }
    auto c=Camera();auto eye=Params();CameraBlock out{};
    CHECK(Build(c,eye,out));CHECK(Near(out.world.position[2],3-0.032f*rotation::UnitsPerMetre));
    CHECK(out.frustum.left==-1.1f&&out.frustum.right==0.9f&&out.frustum.top==1.2f&&out.frustum.bottom==-1);
    CHECK(out.frustum.nearPlane==5&&out.frustum.farPlane==1000);
    eye.eyeMetres[2]=0.032f;CHECK(Build(c,eye,out));CHECK(Near(out.world.position[2],3+0.032f*rotation::UnitsPerMetre));
    eye.head={0,0,1,0,1,0,-1,0,0};CHECK(Build(c,eye,out));CHECK(Near(out.world.position[0],1+0.032f*rotation::UnitsPerMetre));
    CHECK(out.world.rotation[2]==1&&out.world.rotation[6]==-1);
    for(unsigned i=0;i<4;++i){auto invalid=Params();invalid.projection[i]=std::numeric_limits<float>::quiet_NaN();CHECK(!Build(c,invalid,out));invalid=Params();invalid.projection[i]=0;CHECK(!Build(c,invalid,out));}
    for(unsigned i=0;i<3;++i){auto invalid=Params();invalid.eyeMetres[i]=std::numeric_limits<float>::infinity();CHECK(!Build(c,invalid,out));invalid=Params();invalid.headMetres[i]=std::numeric_limits<float>::quiet_NaN();CHECK(!Build(c,invalid,out));}
    auto invalidEye=Params();invalidEye.head[0]=2;CHECK(!Build(c,invalidEye,out));
    for(unsigned i=0;i<7;++i){auto invalid=c;
        if(i==0)invalid.frustum.ortho=1;
        if(i==1)invalid.frustum.nearPlane=0;
        if(i==2)invalid.frustum.farPlane=5;
        if(i==3)invalid.frustum.nearPlane=std::numeric_limits<float>::quiet_NaN();
        if(i==4)invalid.frustum.farPlane=std::numeric_limits<float>::infinity();
        if(i==5)invalid.world.scale=0;
        if(i==6)invalid.world.rotation[0]=2;
        CHECK(!Build(invalid,Params(),out));}
    for(unsigned mask=0;mask<4;++mask){Writer w{mask};const auto r=WriteEye(0x20000,c,Params(),&w,Writer::Write);
        CHECK(r==(!(mask&1)?WriteResult::Applied:(mask&2)?WriteResult::RestoreFailed:WriteResult::Restored));
        CHECK(w.calls==(!(mask&1)?1u:2u));
        if(mask&1){const auto original=Capture(c);CHECK(std::memcmp(&w.actual,&original,144)==0);}}
    Writer w{};CHECK(WriteEye(0x20000,c,invalidEye,&w,Writer::Write)==WriteResult::Invalid&&w.calls==0);
    CHECK(WriteEye(0x20000,c,Params(),&w,nullptr)==WriteResult::Unavailable);
    for(auto address:{0u,0xffffu,UINT32_MAX-0xf7u,UINT32_MAX})CHECK(WriteEye(address,c,Params(),&w,Writer::Write)==WriteResult::Unavailable);
    for(unsigned bits=0;bits<512;++bits){Ops ops{bits};bool ok=Draw(bool(bits&1),ops);std::string expected;bool result=false;
        if(!(bits&1))expected="D";
        else if(!(bits&2))expected="BD";
        else if(!(bits&4))expected="BLUD";
        else if(!(bits&16)||!(bits&64))expected="BLDCU";
        else if(!(bits&8))expected="BLDCURU";
        else if(!(bits&32)||!(bits&128))expected="BLDCURDCU";
        else {expected="BLDCURDCUS";result=bits&256;}
        CHECK(ops.trace==expected);CHECK(ok==result);}
    std::printf("%u stereo checks passed\n",checks);return 0;
}
