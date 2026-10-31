// sprite_pick_math.h
// Rendering: sprite pick math declarations.

#pragma once
#include <DirectXMath.h>
#include <cmath>

inline bool SpriteQuadHit(DirectX::FXMVECTOR origin, DirectX::FXMVECTOR direction,
    DirectX::FXMMATRIX world, DirectX::XMFLOAT2 size, DirectX::XMFLOAT2 pivot) {
    using namespace DirectX;
    if(size.x<=0 || size.y<=0) return false;
    XMVECTOR determinant;
    const auto inverse=XMMatrixInverse(&determinant,world);
    if(std::fabs(XMVectorGetX(determinant))<1e-8f) return false;
    const auto o=XMVector3TransformCoord(origin,inverse);
    const auto d=XMVector3TransformNormal(direction,inverse);
    const float dz=XMVectorGetZ(d);
    if(std::fabs(dz)<1e-6f) return false;
    const float t=-XMVectorGetZ(o)/dz;
    if(t<0) return false;
    const auto hit=XMVectorMultiplyAdd(d,XMVectorReplicate(t),o);
    const float x=XMVectorGetX(hit), y=XMVectorGetY(hit);
    return x>=-pivot.x*size.x && x<=(1-pivot.x)*size.x
        && y>=-pivot.y*size.y && y<=(1-pivot.y)*size.y;
}
