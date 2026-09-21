#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Headset.h"
#include "vr/OpenVRBackend.h"
#include <cstdio>
bool Log(const char* text){std::puts(text);return true;}
int main(){
    nvvr::vr::Configure(Log);obvr::vr::OpenVRBackend backend;
    if(!backend.Start(false)){std::puts("PROBE: runtime unavailable");return 2;}
    unsigned valid=0;
    for(unsigned i=0;i<10;++i){obvr::vr::Quaternion q{};obvr::NiPoint3 p{};bool ok=backend.ReadHeadPose(q,p);if(ok)++valid;
        std::printf("PROBE pose=%u q=(%.5f,%.5f,%.5f,%.5f) pos=(%.4f,%.4f,%.4f)\n",unsigned(ok),q.x,q.y,q.z,q.w,p.x,p.y,p.z);Sleep(100);}
    backend.Stop();return valid?0:3;
}
