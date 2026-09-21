#pragma once
#include <cstdint>
#include <algorithm>
namespace nvvr::resolution {
struct Size { std::uint32_t width=0,height=0; };
struct Arguments { std::uint32_t words[15]{}; };
static_assert(sizeof(Arguments)==60);
// Runtime layout size is distinct from the eye-sized renderer. Never persist
// eye dimensions into the game's INI or its window/input coordinate system.
inline Size layout{};
inline bool Valid(Size s){return s.width>=256 && s.height>=256 && s.width<=16384 && s.height<=16384;}
inline Size Choose(bool available,Size recommended){
    if(!available || !Valid(recommended))return {};
    // Explicit pixel budget for a 32-bit game: preserve the headset aspect
    // while limiting the longest edge to 4096. This is a port policy, not an
    // OpenVR restriction. No invented size when OpenVR has no recommendation.
    const auto longest=(std::max)(recommended.width,recommended.height);
    if(longest>4096){recommended.width=static_cast<std::uint32_t>(std::uint64_t(recommended.width)*4096/longest);recommended.height=static_cast<std::uint32_t>(std::uint64_t(recommended.height)*4096/longest);}
    return Valid(recommended)?recommended:Size{};
}
inline Arguments Prepare(Arguments original,Size wanted){
    if(!Valid(wanted))return original;
    original.words[0]=wanted.width;original.words[1]=wanted.height;
    // EXE E6BF58..69: D3DPRESENT_PARAMETERS.Windowed = !(flags & 4).
    original.words[2]&=~4u;
    original.words[13]=0; // no fullscreen refresh-rate request in windowed mode
    return original;
}
// Retry only the allocating factory, never Initialise on a partly constructed
// renderer. Factory E76210 destroys its failed renderer before returning null.
template<class Create> std::uintptr_t Make(Arguments original,Size wanted,Create create){
    const auto adjusted=Prepare(original,wanted);
    auto result=create(adjusted);
    bool changed=false;for(unsigned i=0;i<15;++i)changed=changed || adjusted.words[i]!=original.words[i];
    if(result || !changed)return result;
    return create(original);
}
}
