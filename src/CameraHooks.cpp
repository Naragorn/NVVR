#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "CameraHooks.h"
#include "HookLogic.h"
#include "PhaseLogic.h"
#include "Rotation.h"
#include "Stereo.h"
#include "ProcessReader.h"
#include <atomic>
#include <mutex>
#include <cstdio>
namespace nvvr::hook {
namespace {
const probe::Reader reader{nullptr,probe::CopyCurrentProcess};
std::atomic<std::uint64_t> frame{0},sequence{0};
std::mutex mutex;
struct Counts { std::uint64_t calls=0,other=0,missing=0,world=0,readFailed=0,worldChanged=0,matrixChanged=0; } counts[2];
struct Row { unsigned site; Address camera; DWORD thread; std::uint64_t begin,end,firstFrame,lastFrame; Sample sample; StackSample stack; rotation::Result rotationResult=rotation::Result::Disabled; };
Row rows[256]; unsigned used=0; std::uint64_t dropped=0;
using MatrixFn=void(__thiscall*)(void*);
#ifdef NVVR_HOOK_TESTS
MatrixFn testOriginal=nullptr;
const probe::Reader* testReader=nullptr;
#endif
__declspec(noinline) void Dispatch(unsigned site,void* camera,MatrixFn original,const probe::Reader& source,Address stack=0) {
    // Preserve the incoming and original-result FP/SIMD state around observation.
    alignas(16) unsigned char fpBefore[512],fpAfter[512];
    __asm fxsave fpBefore
    Row row{};row.site=site;row.camera=static_cast<Address>(reinterpret_cast<std::uintptr_t>(camera));
    row.thread=GetCurrentThreadId();row.begin=++sequence;row.firstFrame=frame.load();
    Before(row.sample,source,row.camera);
    // Capture the bounded stack before the original executes. No raw dereferences.
    if(row.sample.kind==Kind::World) row.stack=ReadStack(reader,stack,site);
    stereo::Baseline(site,row.camera,row.sample);
    if(!stereo::Override(site,row.camera,row.sample))row.rotationResult=rotation::ForCall(site,row.camera,row.sample,source);
    __asm fxrstor fpBefore
    original(camera);
    __asm fxsave fpAfter
    After(row.sample,source,row.camera);
    row.end=++sequence;row.lastFrame=frame.load();
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto& c=counts[site];++c.calls;
        if(row.sample.kind==Kind::MissingWorld) ++c.missing;
        else if(row.sample.kind==Kind::Other) ++c.other;
        else {
            ++c.world;
            if(!row.sample.before || !row.sample.after) ++c.readFailed;
            if(row.sample.worldChanged) ++c.worldChanged;
            if(row.sample.matrixChanged) ++c.matrixChanged;
            if(KeepPhase(c.world,row.firstFrame)) { if(used<256) rows[used++]=row; else ++dropped; }
        }
    }
    __asm fxrstor fpAfter
}
void Entry(unsigned site,void* camera,Address stack) {
#ifdef NVVR_HOOK_TESTS
    if(testOriginal) { Dispatch(site,camera,testOriginal,*testReader,stack);return; }
#endif
    Dispatch(site,camera,reinterpret_cast<MatrixFn>(Original),reader,stack);
}
void __fastcall UpdateImpl(void* camera,void*,Address stack) { Entry(0,camera,stack); }
void __fastcall DirectImpl(void* camera,void*,Address stack) { Entry(1,camera,stack); }
// Capture ESP before any compiler prologue. Third fastcall argument is on stack;
// Impl removes it, leaving the game's original return address for RET.
__declspec(naked) void __fastcall Update(void*,void*) { __asm {
    mov eax,esp
    push eax
    call UpdateImpl
    ret
} }
__declspec(naked) void __fastcall Direct(void*,void*) { __asm {
    mov eax,esp
    push eax
    call DirectImpl
    ret
} }
bool Protect(void*,Address address,std::uint32_t protection,std::uint32_t* previous) {
    DWORD old=0;
    const bool ok=VirtualProtect(reinterpret_cast<void*>(address),5,protection,&old)!=0;
    *previous=old;return ok;
}
void Write(void*,Address address,const void* bytes,std::size_t size) { std::memcpy(reinterpret_cast<void*>(address),bytes,size); }
bool Flush(void*,Address address) { return FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),5)!=0; }
}
void InstallHooks(Logger log) {
    const PatchOps ops{nullptr,probe::CopyCurrentProcess,Protect,Write,Flush};
    const Address targets[]={static_cast<Address>(reinterpret_cast<std::uintptr_t>(&Update)),static_cast<Address>(reinterpret_cast<std::uintptr_t>(&Direct))};
    // Preflight both sites before changing either. Per-site results remain explicit.
    if(!Preflight(reader)) { log("Camera hooks refused: original CALL bytes mismatch");return; }
    for(unsigned i=0;i<2;++i) {
        const auto r=Install(ops,Sites[i],targets[i]); char text[256];
        sprintf_s(text,"camera_hook site=%08X installed=%u mismatch=%u protection_failed=%u flush_failed=%u rollback_flush_failed=%u restore_failed=%u",
            Sites[i],unsigned(r.installed),unsigned(r.mismatch),unsigned(r.protectionFailed),unsigned(r.flushFailed),unsigned(r.rollbackFlushFailed),unsigned(r.restoreFailed));
        log(text);
    }
}
void Present() { ++frame; }
#ifdef NVVR_HOOK_TESTS
void TestDispatch(unsigned site,void* camera,void(__thiscall* original)(void*),const probe::Reader& source) { Dispatch(site,camera,original,source); }
void TestEntry(unsigned site,void* camera,MatrixFn original,const probe::Reader& source) {
    testOriginal=original;testReader=&source;
    if(site==0) Update(camera,nullptr);else Direct(camera,nullptr);
    testOriginal=nullptr;testReader=nullptr;
}
#endif
void Report(Logger log) {
    stereo::Report(log);
    rotation::Report(log);
    // Callback never writes files. Report drains bounded samples from Present/exit.
    std::lock_guard<std::mutex> lock(mutex);char text[1024];
    for(unsigned i=0;i<2;++i) { const auto& c=counts[i];
        sprintf_s(text,"hook_totals site=%08X calls=%llu world=%llu other=%llu missing=%llu read_failed=%llu world_changed=%llu matrix_changed=%llu frame=%llu dropped=%llu",
            Sites[i],c.calls,c.world,c.other,c.missing,c.readFailed,c.worldChanged,c.matrixChanged,frame.load(),dropped);log(text);
    }
    for(unsigned i=0;i<used;++i) { const auto& r=rows[i];
        sprintf_s(text,"hook_sample site=%08X camera=%08X thread=%lu seq=%llu..%llu frame=%llu..%llu before=%u after=%u world_changed=%u matrix_changed=%u world=(%.4f,%.4f,%.4f) matrix00=%.6f->%.6f",
            Sites[r.site],r.camera,r.thread,r.begin,r.end,r.firstFrame,r.lastFrame,unsigned(r.sample.before),unsigned(r.sample.after),unsigned(r.sample.worldChanged),unsigned(r.sample.matrixChanged),
            r.sample.b.world.position[0],r.sample.b.world.position[1],r.sample.b.world.position[2],r.sample.a.worldToCamera[0],r.sample.b.worldToCamera[0]);log(text);
        const auto& f=r.sample.b.frustum;
        sprintf_s(text,"hook_phase seq=%llu site=%08X frame=%llu stack_read=%u caller=%08X outer=%08X parent=%08X frustum=(%.6f,%.6f,%.6f,%.6f;%.4f,%.4f) ortho=%u basis=(%.5f,%.5f,%.5f;%.5f,%.5f,%.5f;%.5f,%.5f,%.5f) rotation=%s",
            r.begin,Sites[r.site],r.firstFrame,unsigned(r.stack.readable),r.stack.caller,r.stack.outer,r.sample.b.parent,
            f.left,f.right,f.top,f.bottom,f.nearPlane,f.farPlane,unsigned(f.ortho),
            r.sample.b.world.rotation[0],r.sample.b.world.rotation[1],r.sample.b.world.rotation[2],r.sample.b.world.rotation[3],r.sample.b.world.rotation[4],r.sample.b.world.rotation[5],r.sample.b.world.rotation[6],r.sample.b.world.rotation[7],r.sample.b.world.rotation[8],rotation::Name(r.rotationResult));log(text);
        // Raw words are evidence, not asserted return addresses. Includes enough
        // surrounding data to inspect the next caller without guessed unwinding.
        sprintf_s(text,"hook_stack seq=%llu",r.begin);std::size_t length=std::strlen(text);
        for(Address word:r.stack.words) length+=static_cast<std::size_t>(sprintf_s(text+length,sizeof(text)-length," %08X",word));
        log(text);
    }
    used=0;
}
}
