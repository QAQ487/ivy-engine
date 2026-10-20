#include "engine/ui/ui_pivot_math.h"
#include <cassert>
#include <initializer_list>
#include <cmath>
int main() {
    using DirectX::XMFLOAT2;
    for(float x:{0.f,.5f,1.f}) for(float y:{0.f,.5f,1.f}) {
        XMFLOAT2 anchor{960,540},position{25,-30},size{200,80},pivot{x,y};
        const auto topLeft=UIPivotTopLeft(anchor,position,size,pivot);
        assert(topLeft.x+x*size.x==985);
        assert(topLeft.y+(1-y)*size.y==510);
        // Left-edge resize: compensate the pivot while keeping the right edge fixed.
        XMFLOAT2 changedSize{250,80},changedPosition{position.x-50+x*50,position.y};
        const auto resized=UIPivotTopLeft(anchor,changedPosition,changedSize,pivot);
        assert(resized.x+changedSize.x==topLeft.x+size.x);
        for(float scale:{.5f,1.f,2.f}) {
            assert(std::fabs((topLeft.x*scale+x*size.x*scale)-(anchor.x+position.x)*scale)<.001f);
            assert(std::fabs((topLeft.y*scale+(1-y)*size.y*scale)-(anchor.y+position.y)*scale)<.001f);
        }
    }
    const auto legacy=UIPivotTopLeft({0,0},{10,20},{200,80},{0,1});
    assert(legacy.x==10 && legacy.y==20);
}
