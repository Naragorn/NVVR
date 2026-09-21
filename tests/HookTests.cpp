#include "Stereo.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "HookLogic.h"
#include "PhaseLogic.h"
#include "CameraHooks.h"
#include <map>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
using namespace nvvr;
using namespace nvvr::hook;
static unsigned checks=0,originalCalls=0;
#define CHECK(x) do { ++checks; if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);} } while(0)
namespace nvvr::hook { void TestDispatch(unsigned,void*,void(__thiscall*)(void*),const probe::Reader&); }
namespace nvvr::hook { void TestEntry(unsigned,void*,void(__thiscall*)(void*),const probe::Reader&); }
struct Memory {
    std::map<Address,std::vector<unsigned char>> bytes;
    unsigned calls=0,fail=0;
    template<class T> void Put(Address a,const T& value) { auto p=reinterpret_cast<const unsigned char*>(&value);bytes[a]={p,p+sizeof(value)}; }
    static bool Read(void* context,Address a,void* out,std::size_t n) {
        auto& m=*static_cast<Memory*>(context);if(++m.calls==m.fail)return false;
        auto it=m.bytes.find(a);if(it==m.bytes.end() || it->second.size()<n)return false;
        std::memcpy(out,it->second.data(),n);return true;
    }
    probe::Reader Reader(){return {this,Read};}
};
constexpr Address Scene=0x20000,Camera=0x30000;
Memory Good(){Memory m;m.Put(probe::WorldGlobal,Scene);m.Put(Scene+0xAC,Camera);probe::Camera c{};c.vtable=probe::CameraVtable;m.Put(Camera,c);return m;}
struct PatchMemory {
    std::array<unsigned char,5> bytes=Call(Sites[0],Original);
    unsigned mask=0,protects=0,flushes=0,writes=0;
    static bool Read(void* p,Address,void* out,std::size_t n) {auto& m=*static_cast<PatchMemory*>(p);if(m.mask&1)return false;std::memcpy(out,m.bytes.data(),n);return true;}
    static bool Protect(void* p,Address,std::uint32_t value,std::uint32_t* old) {
        auto& m=*static_cast<PatchMemory*>(p);++m.protects;CHECK(value==(m.protects==1?0x40u:0x20u));*old=0x20;
        return !(m.mask&(m.protects==1?2u:4u));
    }
    static void Write(void* p,Address,const void* in,std::size_t n){auto& m=*static_cast<PatchMemory*>(p);++m.writes;std::memcpy(m.bytes.data(),in,n);}
    static bool Flush(void* p,Address){auto& m=*static_cast<PatchMemory*>(p);return !(m.mask&(++m.flushes==1?8u:16u));}
    PatchOps Ops(){return {this,Read,Protect,Write,Flush};}
};
static std::string logText;
bool Log(const char* text){logText+=text;logText+='\n';return true;}
Memory* mutate=nullptr;
unsigned mutation=0;
unsigned short seenControl=0,outputControl=0;
bool changeControl=false;
void __fastcall FakeOriginal(void* camera,void*) {
    CHECK(reinterpret_cast<std::uintptr_t>(camera)==Camera);++originalCalls;
    if(mutate){probe::Camera c{};c.vtable=probe::CameraVtable;if(mutation&1)c.world.position[0]=1;if(mutation&2)c.worldToCamera[0]=1;mutate->Put(Camera,c);}
    if(changeControl){__asm fnstcw seenControl
        __asm fldcw outputControl
    }
}
int main(){
    CHECK(Call(Sites[0],Original)==(std::array<unsigned char,5>{0xE8,0xDC,0xF9,0xFF,0xFF}));
    CHECK(Call(Sites[1],Original)==(std::array<unsigned char,5>{0xE8,0x71,0xB8,0x27,0}));
    for(Address site:{0u,0x7fffffffu,0xfffffffdu})for(Address target:{0u,0x12345678u,0xffffffffu}){auto b=Call(site,target);Address d=0;std::memcpy(&d,b.data()+1,4);CHECK(site+5+d==target);}
    for(unsigned mask=0;mask<32;++mask){
        PatchMemory m;m.mask=mask;auto r=Install(m.Ops(),Sites[0],0x12345678);
        CHECK(r.mismatch==bool(mask&1));
        CHECK(r.protectionFailed==(!(mask&1)&&bool(mask&2)));
        const bool write=!(mask&3),flushFail=write&&bool(mask&8);
        CHECK(r.installed==(write&&!flushFail));CHECK(r.flushFailed==flushFail);
        CHECK(r.rollbackFlushFailed==(flushFail&&bool(mask&16)));CHECK(r.restoreFailed==(write&&bool(mask&4)));
        CHECK(m.bytes==Call(Sites[0],r.installed?0x12345678:Original));
        CHECK(m.writes==(write?(flushFail?2u:1u):0u));
    }
    for(unsigned byte=0;byte<5;++byte){PatchMemory m;m.bytes[byte]^=1;CHECK(Install(m.Ops(),Sites[0],0x12345678).mismatch);CHECK(m.writes==0);}
    {Memory m;for(Address site:Sites)m.Put(site,Call(site,Original));CHECK(Preflight(m.Reader()));
        for(unsigned i=1;i<=2;++i){auto b=m;b.calls=0;b.fail=i;CHECK(!Preflight(b.Reader()));}
        for(Address site:Sites)for(unsigned i=0;i<5;++i){auto b=m;b.bytes[site][i]^=1;CHECK(!Preflight(b.Reader()));}
    }
    for(unsigned count=1;count<=1201;++count)CHECK(Keep(count)==(count<=16||count==600||count==1200));
    for(unsigned fail=0;fail<=4;++fail){auto m=Good();m.fail=fail;unsigned calls=0;auto s=Observe(m.Reader(),Camera,[&]{++calls;});CHECK(calls==1);
        CHECK(s.kind==(fail==1||fail==2?Kind::MissingWorld:Kind::World));
        CHECK(s.before==(fail==0||fail==4));CHECK(s.after==(fail==0||fail==3));
        CHECK(!s.worldChanged&&!s.matrixChanged);
    }
    for(Address a:{probe::WorldGlobal,Scene+0xAC}){auto m=Good();m.Put(a,0u);unsigned calls=0;CHECK(Observe(m.Reader(),Camera,[&]{++calls;}).kind==Kind::MissingWorld);CHECK(calls==1);}
    {auto m=Good();unsigned calls=0;CHECK(Observe(m.Reader(),Camera+4,[&]{++calls;}).kind==Kind::Other);CHECK(calls==1);}
    for(unsigned changes=0;changes<4;++changes){auto m=Good();auto s=Observe(m.Reader(),Camera,[&]{probe::Camera c{};c.vtable=probe::CameraVtable;if(changes&1)c.world.position[0]=1;if(changes&2)c.worldToCamera[0]=1;m.Put(Camera,c);});CHECK(s.before&&s.after);CHECK(s.worldChanged==bool(changes&1));CHECK(s.matrixChanged==bool(changes&2));}
    for(unsigned flags=1;flags<4;++flags){auto m=Good();probe::Camera c{};c.vtable=(flags&1)?0:probe::CameraVtable;m.Put(Camera,c);auto s=Observe(m.Reader(),Camera,[&]{c.vtable=(flags&2)?0:probe::CameraVtable;m.Put(Camera,c);});CHECK(s.before==!(flags&1));CHECK(s.after==!(flags&2));CHECK(!s.worldChanged&&!s.matrixChanged);}
    auto original=reinterpret_cast<void(__thiscall*)(void*)>(&FakeOriginal);
    // Execute the same dispatch body as the real wrappers (including FX save/restore).
    auto m=Good();TestDispatch(0,reinterpret_cast<void*>(Camera),original,m.Reader());
    CHECK(originalCalls==1);Present();Report(Log);
    CHECK(logText.find("site=00A711BF calls=1 world=1")!=std::string::npos);
    CHECK(logText.find("before=1 after=1 world_changed=0 matrix_changed=0")!=std::string::npos);
    logText.clear();Report(Log);CHECK(logText.find("hook_sample")==std::string::npos);
    // Outside the full-frame sample window, test periodic sampling overflow.
    Present();Present();
    for(unsigned i=0;i<160000;++i)TestDispatch(1,reinterpret_cast<void*>(Camera),original,m.Reader());
    Report(Log);CHECK(logText.find("dropped=26")!=std::string::npos);
    CHECK(originalCalls==160001);
    {auto other=Good();other.Put(Scene+0xAC,Camera+4);TestDispatch(0,reinterpret_cast<void*>(Camera),original,other.Reader());}
    {Memory missing;TestDispatch(0,reinterpret_cast<void*>(Camera),original,missing.Reader());}
    for(unsigned fail:{3u,4u}){auto broken=Good();broken.fail=fail;TestDispatch(0,reinterpret_cast<void*>(Camera),original,broken.Reader());}
    for(unsigned mode=0;mode<4;++mode){auto changing=Good();mutate=&changing;mutation=mode;TestDispatch(0,reinterpret_cast<void*>(Camera),original,changing.Reader());mutate=nullptr;}
    logText.clear();Report(Log);
    CHECK(logText.find("calls=9 world=7 other=1 missing=1 read_failed=2 world_changed=2 matrix_changed=2")!=std::string::npos);
    unsigned short inputControl=0,afterControl=0;
    __asm fnstcw inputControl
    outputControl=static_cast<unsigned short>((inputControl&~0xC00u)|0x400u);changeControl=true;
    TestDispatch(0,reinterpret_cast<void*>(Camera),original,m.Reader());
    __asm fnstcw afterControl
    __asm fldcw inputControl
    changeControl=false;
    CHECK(seenControl==inputControl);CHECK(afterControl==outputControl);
    for(unsigned site=0;site<2;++site){unsigned oldCalls=originalCalls;auto source=Good();TestEntry(site,reinterpret_cast<void*>(Camera),original,source.Reader());CHECK(originalCalls==oldCalls+1);}
    logText.clear();Report(Log);CHECK(logText.find("stack_read=1")!=std::string::npos);
    CHECK(logText.find("hook_stack seq=")!=std::string::npos);
    CHECK(logText.find("frustum=(")!=std::string::npos);
    for(unsigned f=0;f<1202;++f)for(unsigned count:{1u,16u,17u,599u,600u,601u})
        CHECK(KeepPhase(count,f)==(Keep(count)||f==0||f==1||f==600||f==601||f==1200||f==1201));
    for(Address caller:{0x00A59FB3u,0x00A5A010u,0x00A59D9Cu,0x00A91E51u}){
        Memory stack;Address words[32]{};words[0]=0x00A711C4;words[2]=caller;words[5]=0x123450;words[6]=0x234560;words[7]=0x345670;stack.Put(0x80000,words);
        auto s=ReadStack(stack.Reader(),0x80000,0);CHECK(s.readable);CHECK(s.caller==caller);
        CHECK(s.outer==(caller==0x00A59FB3?words[6]:caller==0x00A5A010?words[7]:caller==0x00A59D9C?words[5]:0));
        s=ReadStack(stack.Reader(),0x80000,1);CHECK(s.readable&&s.caller==0&&s.outer==0);
        words[0]=0;stack.Put(0x80000,words);s=ReadStack(stack.Reader(),0x80000,0);CHECK(s.readable&&s.caller==0&&s.outer==0);
        stack.calls=0;stack.fail=1;CHECK(!ReadStack(stack.Reader(),0x80000,0).readable);
    }
    // Exercise the actual x86 entry wrappers: a matrix-only update must not
    // replace the baseline captured by the fresh-world update wrapper.
    {
        auto source=Good();probe::Camera c{};c.vtable=probe::CameraVtable;
        c.world.position[0]=10;source.Put(Camera,c);
        nvvr::stereo::Reset();Address address=0;probe::Camera saved{};
        CHECK(!nvvr::stereo::TestBaseline(address,saved));
        const unsigned beforeCalls=originalCalls;
        TestEntry(0,reinterpret_cast<void*>(Camera),original,source.Reader());
        CHECK(nvvr::stereo::TestBaseline(address,saved)&&address==Camera&&saved.world.position[0]==10);
        c.world.position[0]=20;source.Put(Camera,c);
        TestEntry(1,reinterpret_cast<void*>(Camera),original,source.Reader());
        CHECK(nvvr::stereo::TestBaseline(address,saved)&&saved.world.position[0]==10);
        CHECK(originalCalls==beforeCalls+2);
        nvvr::stereo::Reset();
        TestEntry(1,reinterpret_cast<void*>(Camera),original,source.Reader());
        CHECK(!nvvr::stereo::TestBaseline(address,saved));
        TestEntry(0,reinterpret_cast<void*>(Camera),original,source.Reader());
        CHECK(nvvr::stereo::TestBaseline(address,saved)&&saved.world.position[0]==20);
        CHECK(originalCalls==beforeCalls+4);
    }
    std::printf("%u hook checks passed\n",checks);
    // MSVC emitted no implicit zero return for this main containing inline asm
    // (verified in HookTests.obj); make the process result explicit.
    return 0;
}
