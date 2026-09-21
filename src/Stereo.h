#pragma once
#include "CameraHooks.h"
#include "HookLogic.h"
namespace nvvr::stereo {
void Install(hook::Logger logger);
void Baseline(unsigned site,probe::Address camera,const hook::Sample& sample);
bool Override(unsigned site,probe::Address camera,const hook::Sample& sample);
#ifdef NVVR_HOOK_TESTS
bool TestBaseline(probe::Address& camera,probe::Camera& value);
#endif
void Reset();
void Report(hook::Logger logger);
bool Gaze(probe::Transform& world);
void Present();
bool FlatRequested();
bool HasSubmitted();
}
