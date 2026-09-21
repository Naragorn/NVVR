#pragma once
#include "HeadPose.h"
#include "CameraHooks.h"
#include "StereoLogic.h"
#include "vr/OpenVRTypes.h"
namespace nvvr::vr {
void Configure(hook::Logger log);
bool PreloadClient();
bool Poll(bool recenter);
bool Pose(std::array<float,9>& local,std::array<float,3>& offset);
void Stop();
bool BeginStereo(stereo::Eye (&eyes)[2],obvr::vr::openvr::HmdMatrix34& renderPose);
bool SubmitStereo(void* left,void* right,const obvr::vr::openvr::HmdMatrix34& renderPose);
void* BeginHud(void* device,unsigned frame);
void EndHud();
void SubmitHud(void* device);
void ResetHud();
bool CaptureCinema(void* device);
void SubmitLayers(void* device,bool flat);
}
