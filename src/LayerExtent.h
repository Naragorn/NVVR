#pragma once
#include "ResolutionLogic.h"
namespace nvvr::ui {
// NVVR changes renderer creation, not merely the D3D backbuffer as OBVR did.
// Its UI target is the current renderer-sized image. The old window size is
// not a valid crop rectangle inside that new image.
inline resolution::Size LayerExtent(resolution::Size renderer,resolution::Size texture){
    if(renderer.width && renderer.height && renderer.width<=texture.width && renderer.height<=texture.height)return renderer;
    return texture;
}
}
