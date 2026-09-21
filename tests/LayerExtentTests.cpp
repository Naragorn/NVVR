#include "LayerExtent.h"
#include "render/EyeGeometry.h"
#include <cstdio>
#include <cstdlib>
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
    using nvvr::resolution::Size;
    for(Size texture:{Size{4036,3376},Size{1176,664}}){
        for(Size renderer:{Size{},Size{0,3376},Size{4036,0},Size{4036,3376},Size{1176,664},Size{4096,3376},Size{4036,4096}}){
            const auto result=nvvr::ui::LayerExtent(renderer,texture);
            const bool valid=renderer.width && renderer.height && renderer.width<=texture.width && renderer.height<=texture.height;
            CHECK(result.width==(valid?renderer.width:texture.width));CHECK(result.height==(valid?renderer.height:texture.height));
        }
    }
    const auto newSize=nvvr::ui::LayerExtent({4036,3376},{4036,3376});
    const auto bounds=obvr::render::ContentBounds(newSize.width,newSize.height,4036,3376);
    CHECK(bounds.uMin==0&&bounds.vMin==0&&bounds.uMax==1&&bounds.vMax==1);
    // Reproduce the regression: the old window dimensions hide >94% of a
    // correctly sized renderer image. This cannot be accepted as a UI crop.
    const auto old=obvr::render::ContentBounds(1176,664,4036,3376);
    CHECK(old.uMax*old.vMax<0.06f);
    std::printf("%u UI extent regression checks passed\n",checks);return 0;
}
