// render_setting_access.h
// Header-only name -> value access for the render settings that tests and scripts may
// change at runtime (test bridge `set_setting`, ScriptRuntimeApi renderSetting*).
//
// Templated on the settings type so Linux tests can use a mock with the same member
// names; the engine instantiates it with RenderSettingsData. Numeric values are
// clamped to the same ranges the scene loader uses, so a caller can never push a value
// the file format would reject. Unknown keys return false and change nothing.

#pragma once

#include "engine/rendering/culling/cull_settings_math.h"
#include "engine/rendering/culling/lod_select_math.h"

#include <cmath>
#include <cstring>

namespace render_setting_access {

// Toggles (JSON bool in the bridge, bool in scripts).
#define RENDER_SETTING_BOOLS(X) \
    X(frustumCullingEnabled) \
    X(shadowCascadeCullingEnabled) \
    X(spriteCullingEnabled) \
    X(lodEnabled) \
    X(animCullingEnabled) \
    X(lightSelectionEnabled) \
    X(billboardCullingEnabled) \
    X(effectCullingEnabled) \
    X(lodDebugLabels)

template <class S>
bool SetBool(S& s, const char* key, bool value) {
    if (!key) return false;
#define X(name) if (std::strcmp(key, #name) == 0) { s.name = value; return true; }
    RENDER_SETTING_BOOLS(X)
#undef X
    return false;
}

template <class S>
bool GetBool(const S& s, const char* key, bool& out) {
    if (!key) return false;
#define X(name) if (std::strcmp(key, #name) == 0) { out = s.name; return true; }
    RENDER_SETTING_BOOLS(X)
#undef X
    return false;
}

// Numbers. NaN / infinity are rejected (return false, value unchanged).
template <class S>
bool SetNumber(S& s, const char* key, double value) {
    if (!key || !std::isfinite(value)) return false;
    const float f = static_cast<float>(value);
    if (std::strcmp(key, "lodBias") == 0) {
        s.lodBias = lod_select::ClampBias(f);
    } else if (std::strcmp(key, "cullDistanceScale") == 0) {
        s.cullDistanceScale = cull_settings::ClampCullDistanceScale(f);
    } else if (std::strcmp(key, "shadowDistance") == 0) {
        s.shadowDistance = cull_settings::ClampShadowDistanceOverride(f);
    } else if (std::strcmp(key, "shadowLodOffset") == 0) {
        s.shadowLodOffset = lod_select::ClampShadowLodOffset(static_cast<int>(std::lround(value)));
    } else if (std::strcmp(key, "effectCullRadius") == 0) {
        s.effectCullRadius = cull_settings::ClampEffectCullRadius(f);
    } else {
        return false;
    }
    return true;
}

template <class S>
bool GetNumber(const S& s, const char* key, double& out) {
    if (!key) return false;
    if (std::strcmp(key, "lodBias") == 0) out = s.lodBias;
    else if (std::strcmp(key, "cullDistanceScale") == 0) out = s.cullDistanceScale;
    else if (std::strcmp(key, "shadowDistance") == 0) out = s.shadowDistance;
    else if (std::strcmp(key, "shadowLodOffset") == 0) out = s.shadowLodOffset;
    else if (std::strcmp(key, "effectCullRadius") == 0) out = s.effectCullRadius;
    else return false;
    return true;
}

} // namespace render_setting_access
