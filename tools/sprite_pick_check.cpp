// sprite_pick_check.cpp
// Validates sprite picking/hit-testing.

#include "engine/rendering/sprite/sprite_pick_math.h"
#include <cassert>
int main() {
    using namespace DirectX;
    const auto ray=XMVectorSet(0,0,-10,1), direction=XMVectorSet(0,0,1,0);
    assert(SpriteQuadHit(ray,direction,XMMatrixIdentity(),{2,2},{0.5f,0.5f}));
    assert(!SpriteQuadHit(XMVectorSet(3,0,-10,1),direction,XMMatrixIdentity(),{2,2},{0.5f,0.5f}));
    assert(!SpriteQuadHit(ray,XMVectorSet(1,0,0,0),XMMatrixIdentity(),{2,2},{0.5f,0.5f}));
    assert(!SpriteQuadHit(ray,-direction,XMMatrixIdentity(),{2,2},{0.5f,0.5f}));
    assert(!SpriteQuadHit(ray,direction,XMMatrixScaling(0,1,1),{2,2},{0.5f,0.5f}));
    const auto parent=XMMatrixRotationZ(XM_PIDIV4)*XMMatrixTranslation(5,3,0);
    const auto world=XMMatrixScaling(-2,3,1)*parent;
    auto local=XMVectorSet(0.25f,0.25f,0,1);
    auto point=XMVector3TransformCoord(local,world);
    auto start=XMVectorSubtract(point,XMVectorSet(0,0,10,0));
    assert(SpriteQuadHit(start,direction,world,{2,2},{0,0}));
    assert(!SpriteQuadHit(start,direction,world,{0,2},{0,0}));
}
