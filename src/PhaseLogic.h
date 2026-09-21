#pragma once
#include "HookLogic.h"
namespace nvvr::hook {
struct StackSample { bool readable=false; Address words[32]{}; Address caller=0,outer=0; };
inline StackSample ReadStack(const probe::Reader& reader,Address stack,unsigned site) {
    StackSample s;
    s.readable=reader.Read(stack,0,s.words,sizeof(s.words));
    if(!s.readable || site!=0 || s.words[0]!=0x00A711C4) return s;
    // Exact offsets from the inspected EXE, not a general stack unwind.
    // At matrix CALL: saved ESI, caller return, UpdateData argument.
    s.caller=s.words[2];
    if(s.caller==0x00A59FB3) s.outer=s.words[6]; // UpdateDownwardPass: ESI, EDI
    else if(s.caller==0x00A5A010) s.outer=s.words[7]; // selected/rigid: EBX, ESI, EDI
    else if(s.caller==0x00A59D9C) s.outer=s.words[5]; // UpdateTransformAndBounds: ESI
    return s;
}
inline bool KeepPhase(std::uint64_t count,std::uint64_t frame) {
    return Keep(count) || frame%600<2;
}
}
