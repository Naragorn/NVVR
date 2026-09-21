#pragma once
#include "HookLogic.h"
#include "CameraHooks.h"
#include "ResolutionLogic.h"
namespace nvvr::resolution {
void Install(hook::Logger log);
#ifdef NVVR_RESOLUTION_TESTS
using CreateFn=std::uintptr_t(__cdecl*)(Arguments);
std::uintptr_t TestCreate(Arguments args,Size wanted,CreateFn original);
#endif
}
