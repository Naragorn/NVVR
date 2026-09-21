#pragma once
#include "CameraHooks.h"
namespace nvvr::ui {
void InstallSizeHooks(hook::Logger log);
#ifdef NVVR_UI_SIZE_TESTS
unsigned TestSizeGetter(bool width,bool ready,unsigned(__cdecl* original)());
#endif
}
