#pragma once
#include "CameraHooks.h"
namespace nvvr::ui {
void Install(hook::Logger log);
void EnsureDevice(hook::Logger log);
void Present();bool HasPresentHook();
void Report(hook::Logger log);
}
