#pragma once
#include "EngineProbe.h"
namespace nvvr::presentation {
// One render invocation is a menu pass when its second stack argument is
// true (870706..730 -> 8707C0), rather than an eye view of the world.
inline bool FlatPass(bool menuPass,bool movie){return menuPass || movie;}
inline bool Cinema(bool requested,bool submitted,bool capturedUi){return requested || (!submitted && !capturedUi);}
inline bool MovieQueued(const probe::Reader& reader){
    probe::Address object=0,vtable=0;std::uint32_t count=0;
    return reader.Value(0x0126FAC4,0,object) && object>=0x10000 &&
        reader.Value(object,0,vtable) && vtable==0x01082564 &&
        reader.Value(object,0x0C,count) && count!=0;
}
// Restoring ownership after a nested Present must not terminate the outer
// frame early. Every original Present still runs exactly once.
template<class Finish,class Original> auto Present(bool owned,bool& inside,Finish finish,Original original){
    if(!owned || inside)return original();
    inside=true;finish();auto result=original();inside=false;return result;
}
}
