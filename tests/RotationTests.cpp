#include "Rotation.h"
#include "HeadPose.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace nvvr;
using namespace nvvr::rotation;
unsigned checks=0;
#define CHECK(x) do {++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
bool Near(float a,float b){return std::fabs(a-b)<0.0001f;}
probe::Transform Identity(){return {{1,0,0,0,1,0,0,0,1},{1,2,3},1};}
struct Memory{
    bool readOk=true;probe::Address collision=0;unsigned writes=0,failMask=0;std::size_t expectedSize=36;float actual[12]{};
    static bool Read(void* p,probe::Address address,void* out,std::size_t size){auto& m=*static_cast<Memory*>(p);CHECK(address==0x2001c&&size==4);if(!m.readOk)return false;std::memcpy(out,&m.collision,4);return true;}
    static bool Write(void* p,probe::Address address,const void* in,std::size_t size){auto& m=*static_cast<Memory*>(p);CHECK(address==0x20068&&size==m.expectedSize);++m.writes;std::memcpy(m.actual,in,size);return !(m.failMask&(1u<<(m.writes-1)));}
    probe::Reader Reader(){return {this,Read};}
};
int main(){
    for(unsigned bits=0;bits<32;++bits){Toggle t{bool(bits&1),bool(bits&2)};bool pressed=bits&4,focused=bits&8,blocked=bits&16;bool before=t.enabled,down=t.down;t.Step(pressed,focused,blocked);CHECK(t.enabled==(!focused||blocked?false:pressed&&!down?!before:before));CHECK(t.down==pressed);t.Reset();CHECK(!t.enabled&&t.down==pressed);}
    std::array<float,9> yaw{};CHECK(Yaw(Identity(),TestAngle,yaw));CHECK(Near(yaw[0],0.98480775f));CHECK(Near(yaw[2],0.17364818f));CHECK(Near(yaw[6],-0.17364818f));
    CHECK(Yaw(Identity(),0,yaw));for(unsigned i=0;i<9;++i)CHECK(yaw[i]==Identity().rotation[i]);
    CHECK(Yaw(Identity(),-TestAngle,yaw));CHECK(yaw[2]<0&&yaw[6]>0);
    for(float angle:{0.2501f,-0.2501f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})CHECK(!Yaw(Identity(),angle,yaw));
    auto bad=Identity();bad.scale=0;CHECK(!Yaw(bad,0,yaw));
    hook::Sample snapshot;snapshot.kind=hook::Kind::World;snapshot.before=true;snapshot.a.world=Identity();
    auto run=[&](Memory& m,bool enabled=true,unsigned site=0){return Apply(enabled,site,0x20000,snapshot,m.Reader(),&m,Memory::Write);};
    Memory m;CHECK(run(m,false)==Result::Disabled&&m.writes==0);CHECK(run(m,true,1)==Result::OtherPath&&m.writes==0);
    snapshot.kind=hook::Kind::Other;CHECK(run(m)==Result::NotWorld);snapshot.kind=hook::Kind::World;
    snapshot.before=false;CHECK(run(m)==Result::Unreadable);snapshot.before=true;
    m.readOk=false;CHECK(run(m)==Result::Unreadable);m.readOk=true;m.collision=1;CHECK(run(m)==Result::Collision);m.collision=0;
    snapshot.a.world=bad;CHECK(run(m)==Result::Invalid);snapshot.a.world=Identity();
    CHECK(Apply(true,0,0x20000,snapshot,m.Reader(),&m,nullptr)==Result::NoWriter);
    for(unsigned mask=0;mask<4;++mask){m=Memory{};m.failMask=mask;auto result=run(m);CHECK(result==(!(mask&1)?Result::Applied:(mask&2)?Result::RestoreFailed:Result::WriteFailedRestored));CHECK(m.writes==(!(mask&1)?1u:2u));if(mask&1)CHECK(std::memcmp(m.actual,snapshot.a.world.rotation,36)==0);}
    // Simulate fresh engine world calculation before every invocation: no accumulation.
    std::array<float,9> expected{};CHECK(Yaw(Identity(),TestAngle,expected));
    for(unsigned frame=0;frame<60;++frame)for(unsigned call=0;call<6;++call){m=Memory{};snapshot.a.world=Identity();CHECK(run(m)==Result::Applied);CHECK(std::memcmp(m.actual,expected.data(),36)==0);}
    std::array<float,9> identity{};std::memcpy(identity.data(),Identity().rotation,36);std::array<float,9> composed{};
    CHECK(Compose(Identity(),identity,composed)&&composed==identity);CHECK(!Compose(bad,identity,composed));identity[0]=2;CHECK(!Compose(Identity(),identity,composed));
    m=Memory{};CHECK(Apply(true,0,0x20000,snapshot,m.Reader(),&m,Memory::Write,&expected)==Result::Applied);CHECK(std::memcmp(m.actual,expected.data(),36)==0);
    m=Memory{};CHECK(Apply(true,0,0x20000,snapshot,m.Reader(),&m,Memory::Write,&identity)==Result::Invalid);
    float target[9]{};const auto address=static_cast<probe::Address>(reinterpret_cast<std::uintptr_t>(target));CHECK(WriteCurrent(nullptr,address,expected.data(),36));CHECK(std::memcmp(target,expected.data(),36)==0);CHECK(!WriteCurrent(nullptr,0,expected.data(),36));
    vr::HeadPose pose;const auto neutral=obvr::vr::Quaternion::Identity();CHECK(!pose.Update(false,neutral,false));CHECK(pose.Update(true,neutral,false));
    for(unsigned i=0;i<9;++i)CHECK(Near(pose.local[i],Identity().rotation[i]));
    CHECK(pose.Update(true,obvr::vr::FromAxisAngle(0,1,0,10),false));for(unsigned i=0;i<9;++i)CHECK(Near(pose.local[i],expected[i]));
    CHECK(pose.Update(true,obvr::vr::FromAxisAngle(0,1,0,10),true));for(unsigned i=0;i<9;++i)CHECK(Near(pose.local[i],Identity().rotation[i]));
    CHECK(pose.Update(true,obvr::vr::FromAxisAngle(0,1,0,10),false));CHECK(Near(pose.local[0],1));
    pose=vr::HeadPose{};CHECK(pose.Update(true,neutral,false));CHECK(pose.Update(true,obvr::vr::FromAxisAngle(1,0,0,10),false));CHECK(pose.local[3]>0); // forward tilts up
    CHECK(pose.Update(true,obvr::vr::FromAxisAngle(0,0,1,10),false));CHECK(pose.local[7]<0); // up tilts left
    CHECK(!pose.Update(true,{0,0,0,0},false));CHECK(!pose.valid);
    CHECK(!pose.Update(true,{std::numeric_limits<float>::quiet_NaN(),0,0,1},false));
    CHECK(!pose.Update(true,{std::numeric_limits<float>::infinity(),0,0,1},false));
    std::array<float,3> moved{},offset{0.1f,0.2f,-0.3f};
    CHECK(Translate(Identity(),offset,moved));
    for(unsigned i=0;i<3;++i)CHECK(Near(moved[i],Identity().position[i]+offset[i]*UnitsPerMetre));
    auto turned=Identity();std::memcpy(turned.rotation,expected.data(),36);
    CHECK(Translate(turned,{1,0,0},moved,1));CHECK(Near(moved[0],1+expected[0])&&Near(moved[2],3+expected[6]));
    CHECK(!Translate(bad,offset,moved));
    for(float invalid:{0.f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})CHECK(!Translate(Identity(),offset,moved,invalid));
    for(unsigned axis=0;axis<3;++axis){auto invalid=offset;invalid[axis]=std::numeric_limits<float>::quiet_NaN();CHECK(!Translate(Identity(),invalid,moved));
        invalid[axis]=std::numeric_limits<float>::max();CHECK(!Translate(Identity(),invalid,moved));}
    for(unsigned mask=0;mask<4;++mask){m=Memory{};m.expectedSize=48;m.failMask=mask;
        const auto r=Apply(true,0,0x20000,snapshot,m.Reader(),&m,Memory::Write,&expected,&offset);
        CHECK(r==(!(mask&1)?Result::Applied:(mask&2)?Result::RestoreFailed:Result::WriteFailedRestored));
        CHECK(m.writes==(!(mask&1)?1u:2u));
        if(mask&1){CHECK(std::memcmp(m.actual,snapshot.a.world.rotation,36)==0);CHECK(std::memcmp(m.actual+9,snapshot.a.world.position,12)==0);}
        else {CHECK(Translate(snapshot.a.world,offset,moved));CHECK(std::memcmp(m.actual+9,moved.data(),12)==0);}}
    m=Memory{};auto invalidOffset=offset;invalidOffset[0]=std::numeric_limits<float>::quiet_NaN();
    CHECK(Apply(true,0,0x20000,snapshot,m.Reader(),&m,Memory::Write,&expected,&invalidOffset)==Result::Invalid&&m.writes==0);
    for(unsigned i=0;i<360;++i){m=Memory{};m.expectedSize=48;CHECK(Apply(true,0,0x20000,snapshot,m.Reader(),&m,Memory::Write,&expected,&offset)==Result::Applied);CHECK(Translate(snapshot.a.world,offset,moved));CHECK(std::memcmp(m.actual+9,moved.data(),12)==0);}
    pose=vr::HeadPose{};CHECK(pose.Update(true,neutral,false,{1,2,3}));for(float v:pose.offset)CHECK(v==0);
    CHECK(pose.Update(true,neutral,false,{1.25f,2.5f,2}));CHECK(Near(pose.offset[0],1)&&Near(pose.offset[1],0.5f)&&Near(pose.offset[2],0.25f));
    const auto heldOffset=pose.offset;
    CHECK(pose.Update(true,obvr::vr::FromAxisAngle(0,1,0,90),false,{1.25f,2.5f,2}));CHECK(pose.offset==heldOffset); // Turning head must not rotate the leaning displacement.
    CHECK(pose.Update(true,obvr::vr::FromAxisAngle(0,1,0,90),true,{1.25f,2.5f,2}));for(float v:pose.offset)CHECK(Near(v,0));
    CHECK(pose.Update(true,neutral,false,{0.25f,2.5f,2}));CHECK(Near(pose.offset[0],1)&&Near(pose.offset[2],0));
    CHECK(!pose.Update(false,neutral,false,{4,5,6})&&!pose.valid);
    for(unsigned axis=0;axis<3;++axis){obvr::NiPoint3 invalid{};(&invalid.x)[axis]=std::numeric_limits<float>::quiet_NaN();CHECK(!pose.Update(true,neutral,true,invalid)&&!pose.valid);}
    for(unsigned i=0;i<10;++i)CHECK(std::strlen(Name(static_cast<Result>(i)))>0);
    std::printf("%u rotation checks passed\n",checks);return 0;
}
