#pragma once
namespace nvvr {
enum class SessionAction { None, ResetWorld, Stop };
// xNVSE lifecycle message IDs from vendor/xnvse-6.4.8/PluginAPI.h.
inline SessionAction OnLifecycle(unsigned type){
    if(type==1||type==7)return SessionAction::Stop;
    if(type==2||type==6||type==14)return SessionAction::ResetWorld;
    return SessionAction::None;
}
}
