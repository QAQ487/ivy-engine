// render_settings.h
// Declares ShadowQuality.

#pragma once

#include "libraries/nlohmann/json.hpp"
#include <DirectXMath.h>
#include <string>
#include "engine/rendering/d3d11/shader.h"
#include "engine/core/builtin_paths.h"

enum class ShadowQuality {
    Low = 0,
    Medium = 1, // current default (4 x 1024, distance 70)
    High = 2
};

struct ShadowQualityPreset {
    unsigned int mapSize = 1024;
    int cascadeCount = 4;
    float maxDistance = 70.0f;
};

struct RenderSettingsData {
    bool skyboxEnabled = true;
    std::string skyboxCubemapPath = "rom/skybox/MySkybox.dds";
    // 0 = auto from image aspect (360 * height / width). Otherwise clamped to (0, 180].
    float skyboxVerticalFov = 0.0f;
    DirectX::XMFLOAT4 skyboxTint = { 1.0f, 1.0f, 1.0f, 1.0f };
    float skyboxExposure = 1.0f;
    float skyboxRotationYDeg = 0.0f;
    bool iblEnabled = true;
    float iblIntensity = 1.0f;
    std::string iblIrradiancePath = "rom/skybox/ibl/irradiance.dds";
    std::string iblPrefilterPath = "rom/skybox/ibl/prefilter.dds";
    // Constant PBR lookup table, compiled into the exe (older scenes may still say rom/skybox/ibl/brdf_lut.png).
    std::string iblBrdfLutPath = builtin_paths::kBrdfLut;
    DirectX::XMFLOAT4 clearColor = { 0.69f, 0.77f, 0.87f, 1.0f };
    std::string defaultShader = "standard";
    ShadowQuality shadowQuality = ShadowQuality::Medium;

    // Height fog (in-shader, opaque lit meshes)
    bool fogEnabled = false;
    DirectX::XMFLOAT3 fogColor = { 0.69f, 0.77f, 0.87f };
    float fogDensity = 0.02f;
    float fogStart = 10.0f;
    float fogHeight = 5.0f;
    float fogHeightFalloff = 0.15f;

    // 3D mesh frustum culling (main camera pass only).
    bool frustumCullingEnabled = true;
    // Per-cascade light-ortho caster culling (open near toward the light).
    bool shadowCascadeCullingEnabled = true;
    // 2D orthographic view-rect sprite culling (render submission only).
    bool spriteCullingEnabled = true;
    // Distance-based mesh LOD (visual.lods). Bias scales distance for level choice
    // only (>1 switches to lower detail sooner); clamped to [0.1, 10] on load.
    bool lodEnabled = true;
    // Frustum-cull camera-facing billboards and individual particles (render submit only).
    bool billboardCullingEnabled = true;
    // Skip drawing Effekseer effects whose bounding sphere (effectCullRadius x scale) is
    // outside the camera frustum. Effects keep simulating. Clamped to [0.5, 500] on load.
    bool effectCullingEnabled = true;
    float effectCullRadius = 20.0f;
    // With more enabled lights than GPU slots (Lighting::MAX_LIGHTS), pick the most
    // influential at the camera instead of the first ones in scene order.
    bool lightSelectionEnabled = true;
    // Skip skeletal pose evaluation for animated Visuals that no pass (main or shadow
    // cascade) drew last frame. Animation time still advances. Play / runtime only.
    bool animCullingEnabled = true;
    float lodBias = 1.0f;
    // Draw a text label above every LOD-tracked mesh with its current level, distance and
    // projected screen height. Debug aid; Play / runtime only.
    bool lodDebugLabels = false;
    // Shadow casters use a lower-detail LOD than the camera pass: this many levels lower
    // (0 = same level, clamped to [0, 3] and to the last available level). Play only.
    int shadowLodOffset = 0;
    // Multiplies every visual.cullDistance (> 1 keeps objects visible further out, < 1 culls
    // sooner). Clamped to [0.1, 10]. Needs lodEnabled, like cullDistance itself.
    float cullDistanceScale = 1.0f;
    // Shadow range override in world units. 0 = use the Shadow Quality preset; positive values
    // replace the preset range and are clamped to [5, 1000].
    float shadowDistance = 0.0f;
};

void RenderSettings_ResetToDefaults();
RenderSettingsData& RenderSettings_Get();
const RenderSettingsData& RenderSettings_GetConst();

// Change / read a runtime-tweakable setting by its JSON-style name (test bridge, scripts).
// Toggles: frustumCullingEnabled, shadowCascadeCullingEnabled, spriteCullingEnabled,
// lodEnabled, animCullingEnabled, lightSelectionEnabled, billboardCullingEnabled,
// effectCullingEnabled, lodDebugLabels. Numbers (clamped like the scene loader): lodBias,
// cullDistanceScale, shadowDistance, shadowLodOffset, effectCullRadius, and lodForceLevel
// (-1 = automatic, 0..7 = force that LOD level; a debug option that is never saved into a
// scene). Unknown keys and non-finite numbers return false and change nothing.
bool RenderSettings_SetBoolByName(const char* key, bool value);
bool RenderSettings_GetBoolByName(const char* key, bool* outValue);
bool RenderSettings_SetNumberByName(const char* key, double value);
bool RenderSettings_GetNumberByName(const char* key, double* outValue);

RenderSettingsData RenderSettings_ParseJson(const nlohmann::json& renderJson, RenderSettingsData settings = {});
void RenderSettings_ApplyFromJson(const nlohmann::json& renderJson);
nlohmann::json RenderSettings_ToJson();

void RenderSettings_ReloadSkybox();
Shader3DVariant RenderSettings_GetDefaultShader();

ShadowQualityPreset RenderSettings_GetShadowPreset();
ShadowQualityPreset RenderSettings_GetShadowPreset(ShadowQuality quality);
const char* RenderSettings_ShadowQualityLabel(ShadowQuality quality);
ShadowQuality RenderSettings_ShadowQualityFromId(const std::string& id);
const char* RenderSettings_ShadowQualityId(ShadowQuality quality);

// Rebuild shadow maps to match current quality (safe to call after D3D init).
void RenderSettings_ApplyShadowQuality();
