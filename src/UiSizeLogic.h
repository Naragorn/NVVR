#pragma once
#include "ResolutionLogic.h"
#include "HookLogic.h"
namespace nvvr::ui {
// These are UI-local callers of the engine's physical width/height getters.
// Window creation and other engine callers retain their original dimensions.
inline constexpr probe::Address WidthGetter=0x004DC1F0,HeightGetter=0x004DC200;
struct SizeSite { probe::Address address,original; };
inline constexpr SizeSite SizeSites[]={
    {0x00700882,WidthGetter},{0x00700890,HeightGetter},
    {0x00700AA1,WidthGetter},{0x00700ABB,HeightGetter},
    {0x00706E24,HeightGetter},{0x00706E54,WidthGetter},
    {0x00707BAD,HeightGetter},
    {0x00715D46,WidthGetter},{0x00715D54,HeightGetter},
    {0x00715DA6,WidthGetter},{0x00715DB4,HeightGetter}
};
inline resolution::Size sourceSize{};
inline resolution::Size SourceSize(resolution::Size frame,resolution::Size window){
    if(!resolution::Valid(frame)||!resolution::Valid(window))return {};
    // Preserve the user's UI aspect, fitting the full layout into the eye texture.
    resolution::Size out=frame;
    if(std::uint64_t(frame.width)*window.height>std::uint64_t(frame.height)*window.width)
        out.width=static_cast<unsigned>((std::uint64_t(frame.height)*window.width+window.height/2)/window.height);
    else
        out.height=static_cast<unsigned>((std::uint64_t(frame.width)*window.height+window.width/2)/window.width);
    return resolution::Valid(out)?out:resolution::Size{};
}
inline unsigned SourceDimension(bool ready,bool width,unsigned native,resolution::Size source){
    if(!ready||!resolution::Valid(source))return native;
    return width?source.width:source.height;
}
template<class Read,class Install> bool InstallSizeSites(Read read,Install install){
    for(const auto& site:SizeSites){
        std::array<unsigned char,5> bytes{};
        if(!read(site.address,bytes)||bytes!=hook::Call(site.address,site.original))return false;
    }
    bool complete=true;
    for(const auto& site:SizeSites){
        const auto r=install(site);
        if(!r.installed||r.restoreFailed||r.flushFailed||r.rollbackFlushFailed)complete=false;
    }
    return complete;
}
}
