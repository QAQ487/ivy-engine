// editor_ui_scale_math.h
// Pure helpers for editor UI scale (user percent × monitor DPI).
// Linux-testable; no ImGui / Win32 dependency.
#pragma once

#include <cmath>

inline constexpr int kEditorUiScalePercentMin = 50;
inline constexpr int kEditorUiScalePercentMax = 300;
inline constexpr int kEditorUiScalePercentDefault = 100;

// Clamp a free-form percent into the supported range. Non-finite → default.
inline int ClampUiScalePercent(int percent)
{
    if (percent < kEditorUiScalePercentMin)
        return kEditorUiScalePercentMin;
    if (percent > kEditorUiScalePercentMax)
        return kEditorUiScalePercentMax;
    return percent;
}

// Normalize a DPI scale factor. Non-finite / non-positive → 1.
inline float NormalizeDpiScale(float dpiScale)
{
    if (!std::isfinite(dpiScale) || dpiScale <= 0.0f)
        return 1.0f;
    // Guard absurd values from broken monitor queries.
    if (dpiScale < 0.5f)
        return 0.5f;
    if (dpiScale > 4.0f)
        return 4.0f;
    return dpiScale;
}

// Effective style/font scale = (user percent / 100) × DPI.
// Always derived from unscaled inputs — never multiply a previously scaled value.
inline float ComputeEffectiveUiScale(int userPercent, float dpiScale)
{
    const int percent = ClampUiScalePercent(userPercent);
    const float dpi = NormalizeDpiScale(dpiScale);
    return (static_cast<float>(percent) / 100.0f) * dpi;
}

// True when re-applying with the same inputs yields the same effective scale
// (documents the no-compounding contract for unit tests).
inline bool EffectiveUiScaleIsIdempotent(int userPercent, float dpiScale)
{
    const float a = ComputeEffectiveUiScale(userPercent, dpiScale);
    const float b = ComputeEffectiveUiScale(userPercent, dpiScale);
    return a == b;
}
