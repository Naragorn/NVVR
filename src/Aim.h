#pragma once
#include "CameraHooks.h"
namespace nvvr::aim {void Install(hook::Logger log);void Report(hook::Logger log);}
#ifdef NVVR_AIM_TESTS
#include "AimLogic.h"
namespace nvvr::aim {
std::uint32_t TestLaunch(Launch args,const probe::Transform& gaze,probe::Address player,std::uint32_t(__cdecl* target)(Launch));
}
#endif
