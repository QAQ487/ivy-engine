// sprite_cull_math.h
// Header-only 2D orthographic view-rect / sprite AABB helpers.
// Pivot convention matches Sprite_DrawWorld / SpriteQuadHit (from bottom-left).
// Linux offline checks compile with the DirectXMath shim.

#pragma once

#include "engine/rendering/culling/frustum_cull_math.h"

namespace sprite_cull {

struct ViewRect {
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    bool valid = false;
};

// Local AABB of the sprite quad on z=0. Pivot is measured from the bottom-left
// of the quad (0,0 = bottom-left, 0.5,0.5 = centre), matching Sprite_DrawWorld:
//   l = -pivot.x * width,  r = l + width
//   b = -pivot.y * height, t = b + height
inline frustum_cull::Aabb MakeSpriteLocalAabb(
    float width, float height, float pivotX, float pivotY) {
    frustum_cull::Aabb box{};
    const float l = -pivotX * width;
    const float r = l + width;
    const float b = -pivotY * height;
    const float t = b + height;
    box.min = {l, b, 0.0f};
    box.max = {r, t, 0.0f};
    // Always valid: degenerate (zero) sizes still produce a point AABB.
    box.valid = true;
    return box;
}

// Axis-aligned world XY view rect for an orthographic camera looking along ±Z
// with up +Y (standard 2D). Matches XMMatrixOrthographicLH(height*aspect, height):
// halfH = orthoHeight/2, halfW = halfH * aspect, centred on (camX, camY).
inline ViewRect MakeOrthoViewRect(
    float camX, float camY, float orthoHeight, float aspect) {
    ViewRect rect{};
    float height = orthoHeight;
    if (!(height > 0.001f))
        height = 0.001f;
    float a = aspect;
    if (!(a > 1e-6f))
        a = 1.0f;
    const float halfH = height * 0.5f;
    const float halfW = halfH * a;
    rect.minX = camX - halfW;
    rect.maxX = camX + halfW;
    rect.minY = camY - halfH;
    rect.maxY = camY + halfH;
    rect.valid = true;
    return rect;
}

inline void InflateViewRect(ViewRect& rect, float margin) {
    if (!rect.valid || !(margin > 0.0f))
        return;
    rect.minX -= margin;
    rect.minY -= margin;
    rect.maxX += margin;
    rect.maxY += margin;
}

// Default edge-popping margin: 2% of the orthographic half-height.
inline float DefaultViewMargin(float orthoHeight) {
    float height = orthoHeight;
    if (!(height > 0.001f))
        height = 0.001f;
    return height * 0.5f * 0.02f;
}

// XY-only intersection against the orthographic view rect.
// Invalid AABB → true (never cull). Invalid / empty view → true.
inline bool AabbIntersectsViewRect(const frustum_cull::Aabb& box, const ViewRect& view) {
    if (!box.valid)
        return true;
    if (!view.valid)
        return true;
    if (box.max.x < view.minX || box.min.x > view.maxX)
        return false;
    if (box.max.y < view.minY || box.min.y > view.maxY)
        return false;
    return true;
}

// World AABB of a sprite via local pivot quad + TRS (degrees).
// Negative / zero scale is handled by TransformAabbTRS corner expand.
inline frustum_cull::Aabb MakeSpriteWorldAabb(
    float width,
    float height,
    float pivotX,
    float pivotY,
    const DirectX::XMFLOAT3& position,
    const DirectX::XMFLOAT3& rotationDeg,
    const DirectX::XMFLOAT3& scale) {
    const frustum_cull::Aabb local = MakeSpriteLocalAabb(width, height, pivotX, pivotY);
    return frustum_cull::TransformAabbTRS(local, position, rotationDeg, scale);
}

#if defined(_WIN32)
// World AABB from an already-built world matrix (includes parent chains /
// renderOffset baked into translation).
inline frustum_cull::Aabb MakeSpriteWorldAabb(
    float width,
    float height,
    float pivotX,
    float pivotY,
    DirectX::FXMMATRIX world) {
    const frustum_cull::Aabb local = MakeSpriteLocalAabb(width, height, pivotX, pivotY);
    return frustum_cull::TransformAabb(local, world);
}
#endif

} // namespace sprite_cull
