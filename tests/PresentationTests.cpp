#include "PresentationLogic.h"
#include "ScreenCopy.h"
#include "vr/OpenVRTypes.h"
#include <cstdio>
#include <cstdlib>
#include <array>
using namespace nvvr;
namespace d=obvr::render::d3d9;
namespace vr=obvr::vr::openvr;
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Memory {
    std::uint32_t object=0x20000,table=0x1082564,movie=0x30000;unsigned fail=0,reads=0;
    static bool Read(void* context,probe::Address address,void* out,std::size_t size){
        auto& m=*static_cast<Memory*>(context);++m.reads;if(m.reads==m.fail)return false;
        CHECK(size==4);std::uint32_t value=0;
        if(address==0x126FAC4)value=m.object;else if(address==m.object)value=m.table;else if(address==m.object+12)value=m.movie;else CHECK(false);
        std::memcpy(out,&value,4);return true;
    }
    probe::Reader Reader(){reads=0;return {this,Read};}
};
struct Object {void** table;};
std::array<void*,120> methods{};std::array<void*,4> surfaceMethods{};
Object device{methods.data()},surface{surfaceMethods.data()},target{surfaceMethods.data()};
unsigned refs=0,copies=0;int failure=0;
UInt32 __stdcall Release(void*){CHECK(refs>0);return --refs;}
SInt32 __stdcall Back(void*,UInt32 chain,UInt32 index,UInt32 type,void** out){
    CHECK(chain==0&&index==0&&type==0);*out=nullptr;
    if(failure==1)return -1;if(failure==2)return 0;
    ++refs;*out=&surface;return failure==3?-1:0;
}
SInt32 __stdcall Copy(void*,void* from,const d::Rect* source,void* to,const d::Rect* dest,UInt32 filter){CHECK(from==&surface&&to==&target&&!source&&!dest&&filter==0);++copies;return failure==4?-1:0;}
int flagResult=0;unsigned flagCalls=0;
int __stdcall SetFlag(vr::VROverlayHandle handle,int flag,bool enabled){++flagCalls;CHECK(handle==123&&flag==4194304&&enabled);return flagResult;}
int main(){
    for(bool menu:{false,true})for(bool movie:{false,true})CHECK(presentation::FlatPass(menu,movie)==(menu||movie));
    for(bool requested:{false,true})for(bool submitted:{false,true})for(bool captured:{false,true})CHECK(presentation::Cinema(requested,submitted,captured)==(requested||(!submitted&&!captured)));
    for(bool owned:{false,true})for(bool nested:{false,true})for(int result:{0,-1,17}){
        bool inside=nested;unsigned finishes=0,originals=0;
        CHECK(presentation::Present(owned,inside,[&]{++finishes;CHECK(inside);},[&]{++originals;return result;})==result);
        CHECK(finishes==(owned&&!nested?1u:0u)&&originals==1&&inside==nested);
    }
    {bool inside=false;unsigned finishes=0,originals=0;
        CHECK(presentation::Present(true,inside,[&]{++finishes;CHECK(presentation::Present(true,inside,[&]{++finishes;},[&]{++originals;return 4;})==4);},[&]{++originals;return 5;})==5);
        CHECK(finishes==1&&originals==2&&!inside);}
    for(unsigned fail=0;fail<4;++fail){Memory m;m.fail=fail;CHECK(presentation::MovieQueued(m.Reader())==(fail==0));}
    for(auto address:{0u,0xFFFFu,0xFFFFFFFFu}){Memory m;m.object=address;CHECK(!presentation::MovieQueued(m.Reader()));}
    {Memory m;m.table=0;CHECK(!presentation::MovieQueued(m.Reader()));m.table=0x1082564;m.movie=0;CHECK(!presentation::MovieQueued(m.Reader()));}
    CHECK(!presentation::MovieQueued({nullptr,nullptr}));
    methods[18]=reinterpret_cast<void*>(Back);methods[34]=reinterpret_cast<void*>(Copy);surfaceMethods[2]=reinterpret_cast<void*>(Release);
    for(failure=0;failure<=4;++failure){refs=0;copies=0;CHECK(presentation::CopyScreen(&device,&target)==(failure==0));CHECK(refs==0);CHECK(copies==(failure==0||failure==4?1u:0u));}
    CHECK(!presentation::CopyScreen(nullptr,&target));CHECK(!presentation::CopyScreen(&device,nullptr));
    methods[18]=nullptr;CHECK(!presentation::CopyScreen(&device,&target));methods[18]=reinterpret_cast<void*>(Back);methods[34]=nullptr;CHECK(!presentation::CopyScreen(&device,&target));
    vr::IVROverlayFnTable table{};CHECK(vr::IgnoreTextureAlpha(nullptr,123)==-1);CHECK(vr::IgnoreTextureAlpha(&table,123)==-1);
    table.SetOverlayFlag=SetFlag;for(int result:{0,10,-1}){flagResult=result;CHECK(vr::IgnoreTextureAlpha(&table,123)==result);}CHECK(flagCalls==3);
    std::printf("%u presentation/movie/screen-copy checks passed\n",checks);return 0;
}
