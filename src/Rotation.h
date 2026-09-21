#pragma once
#include "RotationLogic.h"
#include "CameraHooks.h"
namespace nvvr::rotation {
void Poll(hook::Logger log);
void Reset(hook::Logger log);
Result ForCall(unsigned site,probe::Address camera,const hook::Sample& snapshot,const probe::Reader& reader);
void Report(hook::Logger log);
bool WriteCurrent(void*,probe::Address,const void*,std::size_t);
bool HeadEnabled();
}
