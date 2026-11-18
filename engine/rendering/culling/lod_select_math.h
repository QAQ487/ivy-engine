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

} // namespace lod_select
