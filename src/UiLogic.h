#pragma once
#include <cstdint>
#include <array>
namespace nvvr::ui {
inline constexpr std::array<std::uint32_t,6> ChangedStates{7,168,206,207,208,209};
struct RequestedStates {
    std::array<std::uint32_t,6> values{};
    void Remember(std::uint32_t state,std::uint32_t value){for(unsigned i=0;i<ChangedStates.size();++i)if(ChangedStates[i]==state)values[i]=value;}
};
inline std::uint32_t RenderState(bool capturing,std::uint32_t state,std::uint32_t value){
    if(!capturing)return value;
    switch(state){
        case 7: return 0; // UI has no depth surface.
        case 168:return value|15u;
        case 206:return 1;
        case 207:return 2; // ONE
        case 208:return 6; // INVSRCALPHA
        case 209:return 1; // ADD
        default:return value;
    }
}
template<class Ops> bool Capture(bool enabled,Ops& ops){
    if(!enabled || !ops.Save()){ops.Render();return false;}
    if(!ops.Begin()){ops.Restore();ops.Render();return false;}
    ops.Render();ops.End();return ops.Restore();
}
}
