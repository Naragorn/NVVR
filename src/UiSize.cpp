#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "UiSize.h"
#include "UiSizeLogic.h"
#include "ProcessReader.h"
#include <cstdio>
namespace nvvr::ui {
namespace {
bool sizeReady=false;
using Getter=unsigned(__cdecl*)();
Getter widthOriginal=reinterpret_cast<Getter>(WidthGetter);
Getter heightOriginal=reinterpret_cast<Getter>(HeightGetter);
unsigned __cdecl Width(){return SourceDimension(sizeReady,true,widthOriginal(),sourceSize);}
unsigned __cdecl Height(){return SourceDimension(sizeReady,false,heightOriginal(),sourceSize);}
bool Protect(void*,probe::Address address,std::uint32_t protection,std::uint32_t* previous){
    DWORD old=0;const bool ok=VirtualProtect(reinterpret_cast<void*>(address),5,protection,&old)!=0;*previous=old;return ok;
}
void Write(void*,probe::Address address,const void* bytes,std::size_t size){std::memcpy(reinterpret_cast<void*>(address),bytes,size);}
bool Flush(void*,probe::Address address){return FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),5)!=0;}
}
void InstallSizeHooks(hook::Logger log){
    const probe::Reader reader{nullptr,probe::CopyCurrentProcess};
    const hook::PatchOps ops{nullptr,probe::CopyCurrentProcess,Protect,Write,Flush};
    sizeReady=InstallSizeSites(
        [&](probe::Address address,std::array<unsigned char,5>& bytes){return reader.Read(address,0,bytes.data(),bytes.size());},
        [&](SizeSite site){
            const auto target=site.original==WidthGetter?&Width:&Height;
            const auto r=hook::Install(ops,site.address,static_cast<probe::Address>(reinterpret_cast<std::uintptr_t>(target)),site.original);
            char text[180];sprintf_s(text,"ui_size_hook site=%08X installed=%u restore_failed=%u",site.address,unsigned(r.installed),unsigned(r.restoreFailed));log(text);
            return r;
        });
    log(sizeReady?"ui_size_hooks ready: UI dimension readers share capture layout":"ui_size_hooks disabled: incomplete profile or installation");
}
#ifdef NVVR_UI_SIZE_TESTS
unsigned TestSizeGetter(bool width,bool ready,Getter original){
    sizeReady=ready;if(width){widthOriginal=original;return Width();}
    heightOriginal=original;return Height();
}
#endif
}
