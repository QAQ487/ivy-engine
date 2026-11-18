// cull_settings_math.h
// Header-only helpers for the global culling / shadow-range render settings
// (render.cullDistanceScale, render.shadowDistance). No engine / D3D dependency.

#pragma once

#include <algorithm>
#include <cmath>

namespace cull_settings {

constexpr float kMinCullDistanceScale = 0.1f;
constexpr float kMaxCullDistanceScale = 10.0f;
constexpr float kMinShadowDistance = 5.0f;
constexpr float kMaxShadowDistance = 1000.0f;

// render.cullDistanceScale: multiplies every visual.cullDistance. > 1 keeps objects visible
// further out, < 1 culls sooner. Non-finite input falls back to 1.
inline float ClampCullDistanceScale(float scale) {
    if (!std::isfinite(scale))
        return 1.0f;
    return std::min(std::max(scale, kMinCullDistanceScale), kMaxCullDistanceScale);
}

// Effective cull distance for one object. 0 (never cull) stays 0 whatever the scale.
inline float ScaledCullDistance(float cullDistance, float scale) {
    if (!(cullDistance > 0.0f))
        return 0.0f;
    return cullDistance * ClampCullDistanceScale(scale);
}

// render.shadowDistance: 0 = use the Shadow Quality preset; any positive value overrides the
// preset range and is clamped to [5, 1000]. Non-finite or non-positive input means "preset".
inline float ClampShadowDistanceOverride(float distance) {
    if (!std::isfinite(distance) || distance <= 0.0f)
        return 0.0f;
    return std::min(std::max(distance, kMinShadowDistance), kMaxShadowDistance);
}

// The shadow range actually used for cascade fitting.
inline float ResolveShadowDistance(float presetDistance, float overrideDistance) {
    const float o = ClampShadowDistanceOverride(overrideDistance);
    return o > 0.0f ? o : presetDistance;
}

} // namespace cull_settings
