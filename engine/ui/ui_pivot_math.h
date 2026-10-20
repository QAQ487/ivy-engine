#pragma once
#include <DirectXMath.h>

inline DirectX::XMFLOAT2 UIPivotTopLeft(DirectX::XMFLOAT2 anchor,
    DirectX::XMFLOAT2 position,DirectX::XMFLOAT2 size,DirectX::XMFLOAT2 pivot) {
    return {anchor.x+position.x-pivot.x*size.x,
            anchor.y+position.y-(1-pivot.y)*size.y};
}
