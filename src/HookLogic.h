#pragma once
#include "EngineProbe.h"
#include <array>
namespace nvvr::hook {
using probe::Address;
inline constexpr Address Sites[2]={0x00A711BF,0x007F532A};
inline constexpr Address Original=0x00A70BA0;
inline std::array<unsigned char,5> Call(Address site,Address destination) {
    std::array<unsigned char,5> bytes{0xE8};
    const Address relative=destination-(site+5); // x86 modulo-2^32 displacement
    std::memcpy(bytes.data()+1,&relative,4); return bytes;
}
inline bool Preflight(const probe::Reader& reader) {
    for(Address site:Sites) {
        std::array<unsigned char,5> actual{};
        if(!reader.Read(site,0,actual.data(),actual.size()) || actual!=Call(site,Original)) return false;
    }
    return true;
}
struct PatchOps {
    void* context;
    bool (*read)(void*,Address,void*,std::size_t);
    bool (*protect)(void*,Address,std::uint32_t,std::uint32_t*);
    void (*write)(void*,Address,const void*,std::size_t);
    bool (*flush)(void*,Address);
};
struct PatchResult { bool installed=false, mismatch=false, protectionFailed=false, flushFailed=false, rollbackFlushFailed=false, restoreFailed=false; };
// Only at startup, before these call sites execute. Not a hot-patch algorithm.
inline PatchResult Install(const PatchOps& ops,Address site,Address destination,Address original=Original) {
    PatchResult result;
    const auto expected=Call(site,original), replacement=Call(site,destination);
    std::array<unsigned char,5> current{};
    if(!ops.read(ops.context,site,current.data(),current.size()) || current!=expected) { result.mismatch=true; return result; }
    std::uint32_t old=0,unused=0;
    if(!ops.protect(ops.context,site,0x40,&old)) { result.protectionFailed=true; return result; }
    ops.write(ops.context,site,replacement.data(),replacement.size());
    if(!ops.flush(ops.context,site)) {
        result.flushFailed=true;
        ops.write(ops.context,site,expected.data(),expected.size());
        result.rollbackFlushFailed=!ops.flush(ops.context,site);
    } else result.installed=true;
    result.restoreFailed=!ops.protect(ops.context,site,old,&unused);
    return result;
}
enum class Kind { MissingWorld, Other, World };
inline Kind Identify(const probe::Reader& reader,Address camera) {
    Address scene=0,world=0;
    if(!reader.Value(probe::WorldGlobal,0,scene) || !scene ||
       !reader.Value(scene,0xAC,world) || !world) return Kind::MissingWorld;
    return camera==world ? Kind::World : Kind::Other;
}
struct Sample {
    Kind kind=Kind::MissingWorld;
    bool before=false,after=false,worldChanged=false,matrixChanged=false;
    probe::Camera a{},b{};
};
inline void Before(Sample& s,const probe::Reader& reader,Address camera) {
    s.kind=Identify(reader,camera);
    if(s.kind==Kind::World) s.before=reader.Value(camera,0,s.a) && s.a.vtable==probe::CameraVtable;
}
inline void After(Sample& s,const probe::Reader& reader,Address camera) {
    if(s.kind!=Kind::World) return;
    s.after=reader.Value(camera,0,s.b) && s.b.vtable==probe::CameraVtable;
    if(s.before && s.after) {
        s.worldChanged=std::memcmp(&s.a.world,&s.b.world,sizeof(s.a.world))!=0;
        s.matrixChanged=std::memcmp(s.a.worldToCamera,s.b.worldToCamera,sizeof(s.a.worldToCamera))!=0;
    }
}
template<class OriginalFn> Sample Observe(const probe::Reader& reader,Address camera,OriginalFn original) {
    Sample s; Before(s,reader,camera); original(); After(s,reader,camera); return s;
}
inline bool Keep(std::uint64_t count) { return count<=16 || count%600==0; }
}
