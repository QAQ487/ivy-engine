// fx_cull_math.h
// Header-only bounding-sphere helpers for billboards, particles and Effekseer
// effects. No engine / D3D dependency beyond the frustum plane type.

#pragma once

#include "engine/rendering/culling/frustum_cull_math.h"

#include <algorithm>
#include <cmath>

namespace fx_cull {

// Quads are centred on their position with extents +-0.5 * size, so the
// bounding sphere radius is half the diagonal (rotation and ground alignment
// never reach further).
inline float BillboardRadius(float sizeX, float sizeY) {
    const float w = std::isfinite(sizeX) ? std::fabs(sizeX) : 0.0f;
    const float h = std::isfinite(sizeY) ? std::fabs(sizeY) : 0.0f;
    return 0.5f * std::sqrt(w * w + h * h);
}

// Particle quads are +-0.5 * size (half diagonal 0.7071 * size); a little slack.
inline float ParticleRadius(float size) {
    const float s = std::isfinite(size) ? std::fabs(size) : 0.0f;
    return 0.75f * s;
}

// Effekseer effects have no known bounds: radius is a global guess times the
// largest scale component.
inline float EffectRadius(float baseRadius, float scaleX, float scaleY, float scaleZ) {
    const float sx = std::isfinite(scaleX) ? std::fabs(scaleX) : 1.0f;
    const float sy = std::isfinite(scaleY) ? std::fabs(scaleY) : 1.0f;
    const float sz = std::isfinite(scaleZ) ? std::fabs(scaleZ) : 1.0f;
    return std::max(baseRadius, 0.0f) * std::max({sx, sy, sz, 1e-3f});
}

// False when the sphere is fully outside any frustum plane. Non-finite centres or
// radii never cull (the object is drawn).
inline bool SphereIntersectsFrustum(float cx, float cy, float cz, float radius,
                                    const frustum_cull::FrustumPlanes& frustum) {
    if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(cz) || !std::isfinite(radius))
        return true;
    const float r = std::max(radius, 0.0f);
    for (int i = 0; i < 6; ++i) {
        const DirectX::XMFLOAT4& p = frustum.planes[i];
        if (p.x * cx + p.y * cy + p.z * cz + p.w < -r)
            return false;
    }
    return true;
}

} // namespace fx_cull
