#include "Aim.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace nvvr;
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
aim::Launch expected{};
unsigned calls=0;
unsigned short incoming=0,outgoing=0;
std::uint32_t __cdecl Original(aim::Launch args){
    ++calls;CHECK(std::memcmp(&args,&expected,64)==0);
    unsigned short observed=0;
    __asm fnstcw observed
    CHECK(observed==incoming);
    __asm fldcw outgoing
    return 0x13579BDF;
}
int main(){
    unsigned short saved=0;
    __asm fnstcw saved
    incoming=static_cast<unsigned short>((saved&~0xC00)|0x400);
    outgoing=static_cast<unsigned short>((saved&~0xC00)|0x800);
    probe::Transform gaze{{1,0,0,0,1,0,0,0,1},{0,0,0},1};
    for(unsigned actorMatch=0;actorMatch<2;++actorMatch){
        aim::Launch args{};for(unsigned i=0;i<16;++i)args.words[i]=0x70000+i;
        args.words[0]=0x30000;args.words[1]=actorMatch?0x20000:0x40000;args.words[10]=args.words[11]=0;
        float sy=0.01f,sp=-0.03f;std::memcpy(args.words+13,&sy,4);std::memcpy(args.words+14,&sp,4);
        expected=args;std::array<float,2> angles{};
        __asm fldcw incoming
        if(aim::Angles(true,0x20000,args,gaze,angles))std::memcpy(expected.words+7,angles.data(),8);
        const auto result=aim::TestLaunch(args,gaze,0x20000,Original);
        unsigned short observed=0;
        __asm fnstcw observed
        CHECK(result==0x13579BDF&&observed==outgoing);
    }
    __asm fldcw saved
    CHECK(calls==2);std::printf("%u native aim ABI checks passed\n",checks);return 0;
}
