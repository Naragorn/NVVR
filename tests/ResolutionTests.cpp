#include "UiSizeLogic.h"
#include "Resolution.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
using namespace nvvr::resolution;
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
Arguments expected[2]{};
unsigned calls=0;
bool firstSucceeds=true,secondSucceeds=true;
unsigned short wantedControl=0;
std::uintptr_t __cdecl Original(Arguments args){
    CHECK(calls<2);CHECK(std::memcmp(&args,&expected[calls],sizeof(args))==0);
    unsigned short control=0;__asm fnstcw control
    CHECK(control==wantedControl);
    const bool success=calls++==0?firstSucceeds:secondSucceeds;
    return success?0x12345678u:0;
}
int main(){
    const unsigned values[]={0,1,255,256,257,4095,4096,4097,8192,16384,16385,0xFFFFFFFFu};
    for(auto w:values)for(auto h:values){
        const bool valid=w>=256 && w<=16384 && h>=256 && h<=16384;
        CHECK(Valid({w,h})==valid);
        for(bool available:{false,true}){
            const auto chosen=Choose(available,{w,h});
            if(!available || !valid){CHECK(chosen.width==0&&chosen.height==0);continue;}
            if(w<=4096&&h<=4096){CHECK(chosen.width==w&&chosen.height==h);continue;}
            const auto largest=(std::max)(w,h);
            const auto ew=std::uint64_t(w)*4096/largest,eh=std::uint64_t(h)*4096/largest;
            if(ew<256||eh<256){CHECK(chosen.width==0&&chosen.height==0);}
            else{CHECK(chosen.width==ew&&chosen.height==eh);}
        }
    }
    for(unsigned flags=0;flags<256;++flags){
        Arguments original{};for(unsigned i=0;i<15;++i)original.words[i]=100+i;
        original.words[0]=1176;original.words[1]=664;original.words[2]=flags;
        for(Size wanted:{Size{},Size{256,0},Size{3200,3200},Size{1176,664}}){
            const auto adjusted=Prepare(original,wanted);
            CHECK(original.words[0]==1176&&original.words[1]==664);
            for(unsigned i=0;i<15;++i){
                unsigned expectedWord=original.words[i];
                if(Valid(wanted)){if(i==0)expectedWord=wanted.width;if(i==1)expectedWord=wanted.height;if(i==2)expectedWord=flags&~4u;if(i==13)expectedWord=0;}
                CHECK(adjusted.words[i]==expectedWord);
            }
            for(bool first:{false,true})for(bool second:{false,true}){
                unsigned count=0;
                const auto result=Make(original,wanted,[&](Arguments attempt){
                    CHECK(std::memcmp(&attempt,count==0?&adjusted:&original,sizeof(attempt))==0);
                    return (count++==0?first:second)?std::uintptr_t(77):0;
                });
                const bool changed=std::memcmp(&adjusted,&original,sizeof(original))!=0;
                CHECK(count==((!first&&changed)?2u:1u));
                CHECK(result==((first||(changed&&second))?77u:0u));
            }
        }
    }
    // A size already satisfied in windowed mode must not retry identical input.
    Arguments same{};same.words[0]=same.words[1]=3200;
    unsigned count=0;CHECK(Make(same,{3200,3200},[&](Arguments){++count;return std::uintptr_t(0);})==0);CHECK(count==1);
    // Native compiled cdecl bridge: 15 stack words, return and FPU control.
    unsigned short saved=0;__asm fnstcw saved
    wantedControl=static_cast<unsigned short>((saved&~0xC00)|0x400);
    __asm fldcw wantedControl
    for(Size size:{Size{},Size{3000,3200}})for(bool first:{false,true})for(bool second:{false,true}){
        Arguments args{};for(unsigned i=0;i<15;++i)args.words[i]=0x1000+i;
        args.words[0]=1176;args.words[1]=664;
        expected[0]=Prepare(args,size);expected[1]=args;calls=0;firstSucceeds=first;secondSucceeds=second;
        const auto result=TestCreate(args,size,Original);
        CHECK(result==((first||(Valid(size)&&second))?0x12345678u:0));
        CHECK(calls==((!first&&Valid(size))?2u:1u));
        CHECK(layout.width==1176&&layout.height==664);
        const auto expectedSize=result?nvvr::ui::SourceSize(first&&Valid(size)?size:Size{1176,664},{1176,664}):Size{};
        CHECK(nvvr::ui::sourceSize.width==expectedSize.width&&nvvr::ui::sourceSize.height==expectedSize.height);
    }
    __asm fldcw saved
    std::printf("%u resolution flow and native ABI checks passed\n",checks);return 0;
}
