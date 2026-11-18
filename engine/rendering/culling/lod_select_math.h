// lod_select_math.h
// Header-only distance-based LOD selection (no D3D / DirectXMath dependency).
//
// Levels are numbered 0..count. Level 0 is the Visual's base mesh; level i>0 is
// lods[i-1]. switchDistances[i] is the distance at which rendering switches
// FROM level i TO level i+1 (so `count` thresholds describe `count + 1` levels).

#pragma once

#include <algorithm>
#include <cmath>

namespace lod_select {

constexpr int kCulled = -1;
constexpr int kNoPrevious = -1;
constexpr float kDefaultHysteresis = 0.05f;
constexpr float kMinBias = 0.1f;
constexpr float kMaxBias = 10.0f;

inline float ClampBias(float bias) {
    if (!(bias == bias)) // NaN
        return 1.0f;
    return std::min(std::max(bias, kMinBias), kMaxBias);
}

// Sort thresholds ascending in place and clamp non-finite / negative values to 0.
inline void NormalizeThresholds(float* thresholds, int count) {
    if (!thresholds || count <= 0)
        return;
    for (int i = 0; i < count; ++i) {
        if (!std::isfinite(thresholds[i]) || thresholds[i] < 0.0f)
            thresholds[i] = 0.0f;
    }
    std::sort(thresholds, thresholds + count);
}

// Returns the level index in [0, count], or kCulled when cullDistance > 0 and the
// raw distance exceeds it. `thresholds` must be sorted ascending (see
// NormalizeThresholds). `bias` scales the distance used for level choice only
// (bias > 1 switches to lower detail sooner). `prevLevel` enables hysteresis; pass
// kNoPrevious on first use.
inline int SelectLevel(float distance,
                       const float* thresholds,
                       int count,
                       int prevLevel,
                       float hysteresis,
                       float bias,
                       float cullDistance) {
    if (!std::isfinite(distance) || distance < 0.0f)
        distance = 0.0f;
    if (cullDistance > 0.0f && distance > cullDistance)
        return kCulled;
    if (!thresholds || count <= 0)
        return 0;

    const float d = distance * ClampBias(bias);
    const float h = std::min(std::max(hysteresis, 0.0f), 0.5f);

    if (prevLevel < 0 || prevLevel > count) {
        int level = 0;
        while (level < count && d >= thresholds[level])
            ++level;
        return level;
    }

    int level = prevLevel;
    while (level < count && d >= thresholds[level] * (1.0f + h))
        ++level;
    while (level > 0 && d < thresholds[level - 1] * (1.0f - h))
        --level;
    return level;
}

// Screen-size LOD: a level switches when the object's projected height drops below
// `screenHeight` (fraction of the viewport height, 0..1). The projected height of an
// object with world extent E at distance d is E / (2 d tan(fov/2)), so each threshold
// converts to a switch distance and the normal distance selection (hysteresis, bias)
// applies unchanged. Returns kNeverSwitch when the inputs cannot give a distance
// (no bounds, bad fov, screenHeight <= 0), which keeps the object on its current level.
constexpr float kNeverSwitch = 1.0e30f;

inline float ScreenHeightToDistance(float worldExtent, float tanHalfFov, float screenHeight) {
    if (!std::isfinite(worldExtent) || !std::isfinite(tanHalfFov) || !std::isfinite(screenHeight))
        return kNeverSwitch;
    if (worldExtent <= 0.0f || tanHalfFov <= 0.0f || screenHeight <= 0.0f)
        return kNeverSwitch;
    return worldExtent / (2.0f * tanHalfFov * screenHeight);
}

constexpr int kMaxShadowLodOffset = 3;

inline int ClampShadowLodOffset(int offset) {
    return std::min(std::max(offset, 0), kMaxShadowLodOffset);
}

// Level a shadow caster uses: the camera's level pushed `offset` levels towards lower
// detail, never past the last level (`count` thresholds describe `count + 1` levels).
// A culled visual (kCulled) stays culled.
inline int ShadowCasterLevel(int cameraLevel, int offset, int count) {
    if (cameraLevel < 0)
        return cameraLevel;
    const int last = std::max(count, 0);
    return std::min(cameraLevel + ClampShadowLodOffset(offset), std::max(last, cameraLevel));
}

} // namespace lod_select
