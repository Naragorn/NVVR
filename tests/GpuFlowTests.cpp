#include "FramePair.h"
#include <cstdio>
#include <cstdlib>
#include <array>
#include <cstring>
namespace obvr::log {void Write(const char*,...) {}}
namespace gpu=obvr::render;
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Object {void** table=nullptr;unsigned refs=0,kind=0;};
struct Mock {
    std::array<void*,120> deviceTable{};
    std::array<void*,20> surfaceTable{},textureTable{},interopTable{},imageTable{};
    Object device,back,interop,textures[3],surfaces[3],images[3];
    unsigned creates=0,copies=0,fills=0,transitions=0,locks=0,unlocks=0;
    unsigned failure=0;bool locked=false,placed=false; unsigned cropped=0;
    unsigned layouts[3]{1,1,1};
    static Mock* live;
    static UInt32 __stdcall Add(void* p){return ++static_cast<Object*>(p)->refs;}
    static UInt32 __stdcall Release(void* p){auto& refs=static_cast<Object*>(p)->refs;CHECK(refs>0);return --refs;}
    static SInt32 __stdcall Query(void* p,const gpu::Guid* id,void** out){
        auto& m=*live;auto& object=*static_cast<Object*>(p);*out=nullptr;
        if((object.kind==1&&m.failure==1)||(object.kind!=1&&m.failure==9))return -1;
        if(object.kind==1&&std::memcmp(id,&gpu::kIID_D3D9VkInteropDevice,16)==0)*out=&m.interop;
        else if(object.kind>=20&&object.kind<23&&std::memcmp(id,&gpu::kIID_D3D9VkInteropTexture,16)==0)*out=&m.images[object.kind-20];
        if(*out)Add(*out);return *out?0:-1;
    }
    static SInt32 __stdcall Back(void*,UInt32,UInt32,UInt32,void** out){auto& m=*live;*out=nullptr;if(m.failure==2)return -1;if(m.failure==3)return 0;*out=&m.back;Add(*out);return 0;}
    static SInt32 __stdcall Desc(void*,gpu::d3d9::SurfaceDesc* d){if(live->failure==4)return -1;*d={};d->width=800;d->height=600;d->format=21;return 0;}
    static SInt32 __stdcall Create(void*,UInt32 w,UInt32 h,UInt32 levels,UInt32 usage,UInt32,UInt32 pool,void** out,void**){
        auto& m=*live;*out=nullptr;CHECK(w==800&&h==600&&levels==1&&usage==1&&pool==0);
        if(m.failure==5)return -1;if(m.failure==6)return 0;CHECK(m.creates<3);*out=&m.textures[m.creates++];Add(*out);return 0;
    }
    static SInt32 __stdcall Surface(void* p,UInt32 level,void** out){*out=nullptr;CHECK(level==0);if(live->failure==7)return -1;*out=&live->surfaces[static_cast<Object*>(p)->kind-10];Add(*out);return 0;}
    static SInt32 __stdcall Stretch(void*,void*,const gpu::d3d9::Rect* a,void*,const gpu::d3d9::Rect* b,UInt32 filter){CHECK(!live->locked&&filter==0);CHECK((a==nullptr)==(b==nullptr));if(a){CHECK(live->placed);CHECK(a->left==100&&a->top==75&&a->right==700&&a->bottom==525);CHECK(b->left==0&&b->top==0&&b->right==800&&b->bottom==600);++live->cropped;}++live->copies;return live->failure==8?-1:0;}
    static SInt32 __stdcall Fill(void*,void*,const gpu::d3d9::Rect*,UInt32 colour){CHECK(!live->locked&&colour==0xFF000000u);++live->fills;return live->failure==10?-1:0;}
    static SInt32 __stdcall Info(void* p,gpu::dxvk::VkImageHandle* image,UInt32* layout,gpu::dxvk::VkImageCreateInfo* info){
        CHECK(info->sType==14&&info->queueFamilyIndexCount==0&&info->pQueueFamilyIndices);
        if(live->failure==11)return -1;
        const unsigned i=static_cast<Object*>(p)->kind-30;
        *image=live->failure==12?0:live->failure==20?100:100+i;*layout=live->layouts[i];
        info->format=(live->failure==19&&i==(live->placed?2u:1u))?45:44;info->extent={(live->failure==17&&i==(live->placed?2u:1u))?801u:800u,(live->failure==18&&i==(live->placed?2u:1u))?601u:600u,1};info->samples=live->failure==13?4:1;info->usage=live->failure==14?1:5;return 0;
    }
    static void __stdcall Handles(void*,void** instance,void** physical,void** device){*instance=reinterpret_cast<void*>(1);*physical=reinterpret_cast<void*>(2);*device=live->failure==15?nullptr:reinterpret_cast<void*>(3);}
    static void __stdcall Queue(void*,void** q,UInt32* index,UInt32* family){*q=live->failure==16?nullptr:reinterpret_cast<void*>(4);*index=3;*family=7;}
    static void __stdcall Transition(void*,void* texture,const gpu::dxvk::VkImageSubresourceRange* range,UInt32 oldLayout,UInt32 next){
        auto& m=*live;CHECK(m.locked&&range->aspectMask==1&&range->levelCount==1&&range->layerCount==1);
        const unsigned i=static_cast<Object*>(texture)->kind-30;CHECK(m.layouts[i]==oldLayout);m.layouts[i]=next;++m.transitions;
    }
    static void __stdcall Flush(void*){CHECK(!live->locked);}
    static void __stdcall Lock(void*){CHECK(!live->locked);live->locked=true;++live->locks;}
    static void __stdcall Unlock(void*){CHECK(live->locked);live->locked=false;++live->unlocks;}
    template<class Fn> static void Bind(void*& slot,Fn fn){slot=reinterpret_cast<void*>(fn);}
    Mock(){
        live=this;
        for(auto* table:{deviceTable.data(),surfaceTable.data(),textureTable.data(),interopTable.data(),imageTable.data()}){Bind(table[0],Query);Bind(table[1],Add);Bind(table[2],Release);}
        Bind(deviceTable[18],Back);Bind(deviceTable[23],Create);Bind(deviceTable[34],Stretch);Bind(deviceTable[35],Fill);
        Bind(surfaceTable[12],Desc);Bind(textureTable[18],Surface);
        Bind(interopTable[3],Handles);Bind(interopTable[4],Queue);Bind(interopTable[5],Transition);Bind(interopTable[6],Flush);Bind(interopTable[7],Lock);Bind(interopTable[8],Unlock);
        Bind(imageTable[3],Info);
        device={deviceTable.data(),0,1};back={surfaceTable.data(),0,2};interop={interopTable.data(),0,3};
        for(unsigned i=0;i<3;++i){textures[i]={textureTable.data(),0,10+i};surfaces[i]={surfaceTable.data(),0,20+i};images[i]={imageTable.data(),0,30+i};}
    }
    void Balanced(){CHECK(!locked&&locks==unlocks);CHECK(back.refs==0&&interop.refs==0);for(unsigned i=0;i<3;++i)CHECK(textures[i].refs==0&&surfaces[i].refs==0&&images[i].refs==0);}
};
Mock* Mock::live=nullptr;
int main(){
    for(unsigned failure=0;failure<=20;++failure){Mock m;m.failure=failure;nvvr::stereo::FramePair frames;
        const bool began=frames.Begin(&m.device);CHECK(began==(failure!=1));
        bool submitted=false;
        if(began){const bool left=frames.Copy(0,true);const bool right=left&&frames.Copy(1,true);
            if(right)submitted=frames.Submit([&](void* l,void* r){CHECK(m.locked&&m.layouts[0]==6&&m.layouts[1]==6);auto& a=*static_cast<gpu::dxvk::VRVulkanTextureData*>(l);auto& b=*static_cast<gpu::dxvk::VRVulkanTextureData*>(r);CHECK(a.image!=b.image&&a.queueFamilyIndex==7&&a.width==800&&a.height==600);return true;});}
        CHECK(submitted==(failure==0));frames.Reset();frames.Reset();m.Balanced();
    }
    for(unsigned missing=0;missing<13;++missing){Mock m;
        void** slots[]={&m.deviceTable[0],&m.deviceTable[18],&m.deviceTable[23],&m.textureTable[18],&m.surfaceTable[12],&m.deviceTable[34],&m.deviceTable[35],&m.imageTable[3],&m.interopTable[3],&m.interopTable[4],&m.interopTable[5],&m.interopTable[6],&m.interopTable[7]};
        *slots[missing]=nullptr;nvvr::stereo::FramePair frames;bool submitted=false;
        if(frames.Begin(&m.device)&&frames.Copy(0,true)&&frames.Copy(1,true))submitted=frames.Submit([](void*,void*){return true;});
        CHECK(!submitted);frames.Reset();m.Balanced();
    }
    {Mock m;nvvr::stereo::FramePair frames;CHECK(!frames.Begin(nullptr));CHECK(!frames.Copy(2));CHECK(!frames.Copy(0));CHECK(!frames.Submit([](void*,void*){return true;}));
        CHECK(frames.Begin(&m.device));for(unsigned frame=0;frame<3;++frame){CHECK(frames.Begin(&m.device));CHECK(frames.Copy(0)&&frames.Copy(1));CHECK(!frames.Submit([](void*,void*){return false;}));CHECK(!m.locked&&m.layouts[0]==1&&m.layouts[1]==1);}CHECK(m.creates==2);frames.Reset();m.Balanced();}
    {Mock m;m.layouts[0]=m.layouts[1]=6;nvvr::stereo::FramePair frames;CHECK(frames.Begin(&m.device));CHECK(frames.Copy(0)&&frames.Copy(1));CHECK(frames.Submit([](void*,void*){return true;}));CHECK(m.transitions==0);frames.Reset();m.Balanced();}
    for(int result:{0,1,101}){Mock m;unsigned called=0;
        CHECK(nvvr::stereo::WaitUnderQueue(&m.device,[&]{++called;CHECK(m.locked);return result;})==(result==0));
        CHECK(called==1);m.Balanced();}
    {Mock m;bool called=false;CHECK(!nvvr::stereo::WaitUnderQueue(nullptr,[&]{called=true;return 0;}));CHECK(!called);m.Balanced();}
    // Exercise both the whole-surface resolve and the subsequent optical crop,
    // including each existing resource/interop failure and absent method.
    for(unsigned failure=0;failure<=20;++failure){Mock m;m.failure=failure;m.placed=true;nvvr::stereo::FramePair frames;
        nvvr::stereo::Placement p{};p.sourceUMin=p.sourceVMin=0.125f;p.sourceUMax=p.sourceVMax=0.875f;
        bool submitted=false;
        if(frames.Begin(&m.device)&&frames.Copy(0,false,&p)&&frames.Copy(1,false,&p))
            submitted=frames.Submit([&](void* l,void* r){
                CHECK(m.locked&&m.layouts[1]==6&&m.layouts[2]==6);
                CHECK(static_cast<gpu::dxvk::VRVulkanTextureData*>(l)->image==101);
                CHECK(static_cast<gpu::dxvk::VRVulkanTextureData*>(r)->image==102);return true;
            });
        CHECK(submitted==(failure==0));
        if(!failure){CHECK(m.creates==3&&m.copies==4&&m.cropped==2&&m.fills==2);
            CHECK(frames.Begin(&m.device));CHECK(frames.Copy(0,false,&p)&&frames.Copy(1,false,&p));CHECK(m.creates==3);}
        frames.Reset();m.Balanced();
    }
    for(unsigned missing=0;missing<13;++missing){Mock m;m.placed=true;
        void** slots[]={&m.deviceTable[0],&m.deviceTable[18],&m.deviceTable[23],&m.textureTable[18],&m.surfaceTable[12],&m.deviceTable[34],&m.deviceTable[35],&m.imageTable[3],&m.interopTable[3],&m.interopTable[4],&m.interopTable[5],&m.interopTable[6],&m.interopTable[7]};
        *slots[missing]=nullptr;nvvr::stereo::FramePair frames;bool submitted=false;
        nvvr::stereo::Placement p{};p.sourceUMin=p.sourceVMin=0.125f;p.sourceUMax=p.sourceVMax=0.875f;
        if(frames.Begin(&m.device)&&frames.Copy(0,false,&p)&&frames.Copy(1,false,&p))submitted=frames.Submit([](void*,void*){return true;});
        CHECK(!submitted);frames.Reset();m.Balanced();
    }
    {Mock m;nvvr::stereo::FramePair frames;nvvr::stereo::Placement p{};p.sourceUMax=0;
        CHECK(frames.Begin(&m.device));CHECK(!frames.Copy(0,false,&p));CHECK(m.creates==0&&m.copies==0);frames.Reset();m.Balanced();}
    // All combinations of image alias, width, height, format and sample mismatch.
    for(unsigned bits=0;bits<32;++bits){
        gpu::BackBufferImage a{},b{};a.image=1;b.image=(bits&1)?1:2;
        a.width=800;b.width=(bits&2)?801:800;a.height=600;b.height=(bits&4)?601:600;
        a.format=44;b.format=(bits&8)?45:44;a.sampleCount=1;b.sampleCount=(bits&16)?4:1;
        CHECK(nvvr::stereo::MatchingImages(a,b)==(bits==0));
    }
    std::printf("%u GPU flow checks passed\n",checks);return 0;
}
