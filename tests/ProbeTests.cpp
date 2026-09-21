#include "ProcessReader.h"
#include <map>
#include <vector>
#include <limits>
#include <cstdio>
#include <cstdlib>
using namespace nvvr::probe;
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
struct Memory {
    std::map<Address, std::vector<unsigned char>> blocks;
    unsigned calls=0, fail=0, change=0;
    template<class T> void Put(Address a,const T& v) { const auto p=reinterpret_cast<const unsigned char*>(&v); blocks[a]={p,p+sizeof(v)}; }
    static bool Copy(void* c,Address a,void* out,std::size_t n) {
        auto& m=*static_cast<Memory*>(c); ++m.calls;
        if(m.calls==m.fail) return false;
        auto it=m.blocks.find(a); if(it==m.blocks.end() || it->second.size()<n) return false;
        std::memcpy(out,it->second.data(),n);
        if(m.calls==m.change) { CHECK(n==sizeof(Address)); *static_cast<Address*>(out)+=4; }
        return true;
    }
    Reader reader() { return {this,Copy}; }
};
constexpr Address R=0x20000,D=0x30000,V=0x40000,S=0x50000,C=0x60000;
Transform GoodTransform() { return {{1,0,0,0,1,0,0,0,1},{1,2,3},1}; }
Frustum GoodFrustum() { return {-1,1,1,-1,1,1000,0,{}}; }
Camera GoodCamera() { Camera c{}; c.vtable=CameraVtable; c.local=c.world=GoodTransform(); c.frustum=GoodFrustum(); return c; }
Memory Good() {
    Memory m; m.Put(RendererGlobal,R); m.Put(RendererAlias,R); m.Put(R,RendererVtable);
    m.Put(R+0x288,D); m.Put(R+0xA98,1920u); m.Put(R+0xA9C,1080u); m.Put(D,V); m.Put(V,0x70000u);
    m.Put(WorldGlobal,S); m.Put(S,SceneVtable); m.Put(S+0xAC,C); m.Put(C,GoodCamera()); return m;
}
int main() {
    auto m=Good(); Address out=0; auto r=m.reader();
    CHECK(r.Value(RendererGlobal,0,out) && out==R);
    CHECK(!r.Read(1,0,&out,4)); CHECK(!r.Read(R,0,nullptr,4)); CHECK(!r.Read(R,0,&out,0));
    CHECK(!r.Read(UINT32_MAX,1,&out,1)); CHECK(!r.Read(UINT32_MAX,0,&out,2));
    Reader empty{nullptr,nullptr}; CHECK(!empty.Value(R,0,out));
    m.Put(UINT32_MAX,static_cast<unsigned char>(7)); unsigned char byte=0;
    CHECK(r.Read(UINT32_MAX,0,&byte,1) && byte==7);
    CHECK(!r.Value(0x90000,0,out)); CHECK(!r.Read(RendererGlobal,0,&out,5));
    CHECK(CopyCurrentProcess(nullptr,static_cast<Address>(reinterpret_cast<std::uintptr_t>(&out)),&out,sizeof(out)));
    CHECK(!CopyCurrentProcess(nullptr,0,&out,sizeof(out)));
    Memory signatures;
    for(const auto& sig:Signatures) signatures.blocks[sig.address]={sig.bytes,sig.bytes+sig.length};
    CHECK(!VerifyProfile(signatures.reader(),0)); CHECK(VerifyProfile(signatures.reader(),0x400000));
    for(unsigned i=1;i<=5;++i) { auto x=signatures; x.calls=0; x.fail=i; CHECK(!VerifyProfile(x.reader(),0x400000)); }
    for(const auto& sig:Signatures) for(std::size_t j=0;j<sig.length;++j) {
        auto x=signatures; x.blocks[sig.address][j]^=1; CHECK(!VerifyProfile(x.reader(),0x400000));
    }
    CHECK(PlausibleTransform(GoodTransform())); CHECK(PlausibleFrustum(GoodFrustum()));
    const float nan=std::numeric_limits<float>::quiet_NaN();
    for(unsigned i=0;i<9;++i) { auto t=GoodTransform(); t.rotation[i]=nan; CHECK(!PlausibleTransform(t)); }
    for(unsigned i=0;i<3;++i) { auto t=GoodTransform(); t.position[i]=nan; CHECK(!PlausibleTransform(t)); }
    for(float scale:{nan,0.f,-1.f}) { auto t=GoodTransform(); t.scale=scale; CHECK(!PlausibleTransform(t)); }
    for(unsigned row=0;row<3;++row) { auto t=GoodTransform(); t.rotation[row*3+row]=2; CHECK(!PlausibleTransform(t)); }
    for(unsigned row=1;row<3;++row) for(unsigned other=0;other<row;++other) {
        auto t=GoodTransform(); for(unsigned col=0;col<3;++col) t.rotation[row*3+col]=t.rotation[other*3+col]; CHECK(!PlausibleTransform(t));
    }
    { auto t=GoodTransform(); t.rotation[0]=0;t.rotation[1]=-1;t.rotation[3]=1;t.rotation[4]=0; CHECK(PlausibleTransform(t)); }
    for(unsigned i=0;i<6;++i) { auto f=GoodFrustum(); float* fields[]={&f.left,&f.right,&f.top,&f.bottom,&f.nearPlane,&f.farPlane}; *fields[i]=nan; CHECK(!PlausibleFrustum(f)); }
    for(unsigned i=0;i<5;++i) { auto f=GoodFrustum(); if(i==0) f.left=f.right; if(i==1) f.bottom=f.top; if(i==2) f.nearPlane=0; if(i==3) f.farPlane=f.nearPlane; if(i==4) f.ortho=1; CHECK(!PlausibleFrustum(f)); }
    m=Good(); CHECK(ReadRenderer(m.reader()).state==State::Ready); CHECK(m.calls==10);
    for(unsigned i=1;i<=10;++i) { m=Good();m.fail=i; CHECK(ReadRenderer(m.reader()).state==(i==1?State::Ready:State::Unreadable)); }
    for(unsigned i:{9u,10u}) { m=Good();m.change=i;CHECK(ReadRenderer(m.reader()).state==State::Changed); }
    for(Address a:{RendererGlobal,R+0x288}) { m=Good();m.Put(a,0u); CHECK(ReadRenderer(m.reader()).state==State::Absent); }
    m=Good();m.Put(R,0u);CHECK(ReadRenderer(m.reader()).state==State::WrongType);
    m=Good();m.Put(V,0u);CHECK(ReadRenderer(m.reader()).state==State::InvalidValues);
    for(Address a:{R+0xA98,R+0xA9C}) for(unsigned v:{0u,32769u}) { m=Good();m.Put(a,v);CHECK(ReadRenderer(m.reader()).state==State::InvalidValues); }
    m=Good();m.Put(R+0xA98,32768u);m.Put(R+0xA9C,32768u);CHECK(ReadRenderer(m.reader()).state==State::Ready);
    m=Good();CHECK(ReadCamera(m.reader()).state==State::Ready);CHECK(m.calls==6);
    for(unsigned i=1;i<=6;++i) { m=Good();m.fail=i;CHECK(ReadCamera(m.reader()).state==State::Unreadable); }
    for(unsigned i:{5u,6u}) { m=Good();m.change=i;CHECK(ReadCamera(m.reader()).state==State::Changed); }
    for(Address a:{WorldGlobal,S+0xAC}) { m=Good();m.Put(a,0u); CHECK(ReadCamera(m.reader()).state==State::Absent); }
    m=Good();m.Put(S,0u);CHECK(ReadCamera(m.reader()).state==State::WrongType);
    m=Good();auto c=GoodCamera();c.vtable=0;m.Put(C,c);CHECK(ReadCamera(m.reader()).state==State::WrongType);
    for(unsigned i=0;i<3;++i) { m=Good();c=GoodCamera();if(i==0)c.local.scale=0;if(i==1)c.world.scale=0;if(i==2)c.frustum.ortho=1;m.Put(C,c);CHECK(ReadCamera(m.reader()).state==State::InvalidValues); }
    for(State s:{State::Absent,State::Unreadable,State::WrongType,State::InvalidValues,State::Changed,State::Ready}) CHECK(std::strlen(Name(s))>0);
    std::printf("%u probe checks passed\n",checks);
}
