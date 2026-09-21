#pragma once
#include "CameraHooks.h"
#include "UiPickLogic.h"
namespace nvvr::ui {
void InstallPickHook(hook::Logger log);
void SetPickLayout(PickLayout layout);
#ifdef NVVR_UI_PICK_TESTS
using PickNative=bool(__thiscall*)(void*,unsigned,unsigned,void*,void*,void*);
using PickRay=void(__thiscall*)(const void*,float,float,void*,void*);
bool TestPick(void* camera,unsigned x,unsigned y,void* origin,void* direction,void* target,
              PickNative native,PickRay ray,const PickPlan& plan);
#endif
}
