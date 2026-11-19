// shadow_mode.h
// Scene shadow technique (settings.render.shadowMode) and its clamps. Pure, header-only;
// Linux-tested by tests/shadow_mode_test.cpp.
//
//   "csm"    cascaded shadow maps (default; scenes without the key keep this)
//   "volume" stencil shadow volumes for Visuals with "shadowVolume": true; CSM is off
//   "none"   no shadows
#pragma once

#include <cmath>
#include <string>

enum class ShadowMode {
    Csm = 0,
    Volume = 1,
    None = 2,
};

namespace shadow_mode {

inline const char* Id(ShadowMode mode) {
    switch (mode) {
    case ShadowMode::Volume: return "volume";
    case ShadowMode::None: return "none";
    case ShadowMode::Csm:
    default: return "csm";
    }
}

inline const char* Label(ShadowMode mode) {
    switch (mode) {
    case ShadowMode::Volume: return "Shadow Volume (stencil)";
    case ShadowMode::None: return "None";
    case ShadowMode::Csm:
    default: return "Cascaded Shadow Maps";
    }
}

// Unknown ids fall back to CSM, like an unknown shadowQuality falls back to Medium.
inline ShadowMode FromId(const std::string& id) {
    if (id == "volume") return ShadowMode::Volume;
    if (id == "none") return ShadowMode::None;
    return ShadowMode::Csm;
}

// How much a shadow-volume shadow darkens the lit image: 0 = invisible, 1 = black.
constexpr float kDefaultVolumeStrength = 0.55f;

inline float ClampVolumeStrength(float v) {
    if (!std::isfinite(v)) return kDefaultVolumeStrength;
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

}  // namespace shadow_mode
