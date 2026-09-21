#pragma once
namespace nvvr::rotation {
inline constexpr int RecenterKey=0x2E; // VK_DELETE, Windows SDK WinUser.h
struct VrActions {bool poll=false,stop=false,recenter=false;};
struct AlwaysVr {
    bool enabled=false,centerDown=false;
    VrActions Step(bool focused,bool blocked,bool pressed){
        const bool previous=enabled;
        enabled=!blocked;
        const bool center=enabled&&focused&&pressed&&!centerDown;
        // Track the physical key even while unfocused/blocked: returning to
        // the game with Delete held must not unexpectedly move the anchor.
        centerDown=pressed;
        return {enabled,previous&&!enabled,center};
    }
    void Reset(){enabled=false;}
};
}
