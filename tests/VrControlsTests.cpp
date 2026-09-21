#include "VrControls.h"
#include <cstdio>
#include <cstdlib>
using namespace nvvr::rotation;
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
    static_assert(RecenterKey==0x2E);static_assert(RecenterKey!=0x79&&RecenterKey!=0x7A);
    // Every state/input combination, followed by a lifecycle reset and repeat.
    for(unsigned mask=0;mask<32;++mask){
        const bool was=mask&1,held=mask&2,focused=mask&4,blocked=mask&8,pressed=mask&16;
        AlwaysVr state{was,held};const auto a=state.Step(focused,blocked,pressed);
        CHECK(a.poll==!blocked);CHECK(a.stop==(was&&blocked));
        CHECK(a.recenter==(!blocked&&focused&&pressed&&!held));
        CHECK(state.enabled==!blocked&&state.centerDown==pressed);
        state.Reset();CHECK(!state.enabled&&state.centerDown==pressed);
        const auto b=state.Step(focused,blocked,pressed);
        CHECK(b.poll==!blocked);CHECK(!b.stop&&!b.recenter);
    }
    // Exhaustive four-frame sequences: focus, fault and key transitions.
    for(unsigned sequence=0;sequence<4096;++sequence){
        AlwaysVr state{};bool previousEnabled=false,previousKey=false;
        for(unsigned frame=0;frame<4;++frame){
            const unsigned input=(sequence>>(frame*3))&7;
            const bool focused=input&1,blocked=input&2,key=input&4;
            const auto a=state.Step(focused,blocked,key);
            CHECK(a.poll==!blocked);CHECK(a.stop==(previousEnabled&&blocked));
            CHECK(a.recenter==(focused&&!blocked&&key&&!previousKey));
            previousEnabled=!blocked;previousKey=key;
        }
    }
    std::printf("%u automatic VR control checks passed\n",checks);
}
