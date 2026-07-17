#include "render_settings.h"

#include "engine/rendering/environment/skybox.h"
#include "engine/rendering/d3d11/shader.h"
#include "engine/rendering/d3d11/renderer.h"
#include "engine/rendering/lighting/shadow.h"

static RenderSettingsData g_renderSettings;

void RenderSettings_ResetToDefaults() {
    g_renderSettings = {};
}

RenderSettingsData& RenderSettings_Get() {
    return g_renderSettings;
}

const RenderSettingsData& RenderSettings_GetConst() {
    return g_renderSettings;
}

static DirectX::XMFLOAT4 ReadColor4(const nlohmann::json& j, const DirectX::XMFLOAT4& fallback) {
    if (!j.is_array() || j.size() < 4) return fallback;
    return {
        j[0].get<float>(),
        j[1].get<float>(),
        j[2].get<float>(),
        j[3].get<float>()
    };
}

static DirectX::XMFLOAT3 ReadColor3(const nlohmann::json& j, const DirectX::XMFLOAT3& fallback) {
    if (!j.is_array() || j.size() < 3) return fallback;
    return {
        j[0].get<float>(),
        j[1].get<float>(),
        j[2].get<float>()
    };
}

ShadowQualityPreset RenderSettings_GetShadowPreset(ShadowQuality quality) {
    ShadowQualityPreset p {};
    switch (quality) {
    case ShadowQuality::Low:
        p.mapSize = 512;
        p.cascadeCount = 2;
        p.maxDistance = 40.0f;
        break;
    case ShadowQuality::High:
        p.mapSize = 2048;
        p.cascadeCount = 4;
        p.maxDistance = 90.0f;
        break;
    case ShadowQuality::Medium:
    default:
        p.mapSize = 1024;
        p.cascadeCount = 4;
        p.maxDistance = 70.0f;
        break;
    }
    return p;
}

ShadowQualityPreset RenderSettings_GetShadowPreset() {
    return RenderSettings_GetShadowPreset(g_renderSettings.shadowQuality);
}

const char* RenderSettings_ShadowQualityLabel(ShadowQuality quality) {
    switch (quality) {
    case ShadowQuality::Low: return "Low";
    case ShadowQuality::High: return "High";
    case ShadowQuality::Medium:
    default: return "Medium";
    }
}

const char* RenderSettings_ShadowQualityId(ShadowQuality quality) {
    switch (quality) {
    case ShadowQuality::Low: return "low";
    case ShadowQuality::High: return "high";
    case ShadowQuality::Medium:
    default: return "medium";
    }
}

ShadowQuality RenderSettings_ShadowQualityFromId(const std::string& id) {
    if (id == "low" || id == "Low" || id == "0")
        return ShadowQuality::Low;
    if (id == "high" || id == "High" || id == "2")
        return ShadowQuality::High;
    return ShadowQuality::Medium;
}

void RenderSettings_ApplyShadowQuality() {
    if (!GetDevice() || !GetDeviceContext())
        return;

    const ShadowQualityPreset preset = RenderSettings_GetShadowPreset();
    const DirectX::XMFLOAT3 dir = Shadow::GetLightDirection(0);
    const bool backface = Shadow::IsBackfaceDepthEnabled();

    if (!Shadow::Reconfigure(preset.mapSize, preset.cascadeCount))
        return;

    Shadow::SetLightDirection(0, dir);
    Shadow::SetBackfaceDepth(backface);
}

void RenderSettings_ApplyFromJson(const nlohmann::json& renderJson) {
    if (!renderJson.is_object()) return;

    g_renderSettings.skyboxEnabled = renderJson.value("skyboxEnabled", g_renderSettings.skyboxEnabled);
    g_renderSettings.skyboxCubemapPath = renderJson.value(
        "skyboxCubemap", g_renderSettings.skyboxCubemapPath);
    g_renderSettings.skyboxTint = ReadColor4(
        renderJson["skyboxTint"], g_renderSettings.skyboxTint);
    g_renderSettings.skyboxExposure = renderJson.value(
        "skyboxExposure", g_renderSettings.skyboxExposure);
    g_renderSettings.skyboxRotationYDeg = renderJson.value(
        "skyboxRotationYDeg", g_renderSettings.skyboxRotationYDeg);
    g_renderSettings.clearColor = ReadColor4(
        renderJson["clearColor"], g_renderSettings.clearColor);
    g_renderSettings.defaultShader = renderJson.value(
        "defaultShader", g_renderSettings.defaultShader);

    if (renderJson.contains("shadowQuality")) {
        if (renderJson["shadowQuality"].is_string())
            g_renderSettings.shadowQuality = RenderSettings_ShadowQualityFromId(
                renderJson["shadowQuality"].get<std::string>());
        else if (renderJson["shadowQuality"].is_number_integer()) {
            const int q = renderJson["shadowQuality"].get<int>();
            if (q <= 0) g_renderSettings.shadowQuality = ShadowQuality::Low;
            else if (q >= 2) g_renderSettings.shadowQuality = ShadowQuality::High;
            else g_renderSettings.shadowQuality = ShadowQuality::Medium;
        }
    }

    g_renderSettings.fogEnabled = renderJson.value("fogEnabled", g_renderSettings.fogEnabled);
    if (renderJson.contains("fogColor"))
        g_renderSettings.fogColor = ReadColor3(renderJson["fogColor"], g_renderSettings.fogColor);
    g_renderSettings.fogDensity = renderJson.value("fogDensity", g_renderSettings.fogDensity);
    g_renderSettings.fogStart = renderJson.value("fogStart", g_renderSettings.fogStart);
    g_renderSettings.fogHeight = renderJson.value("fogHeight", g_renderSettings.fogHeight);
    g_renderSettings.fogHeightFalloff = renderJson.value(
        "fogHeightFalloff", g_renderSettings.fogHeightFalloff);

    RenderSettings_ReloadSkybox();
    RenderSettings_ApplyShadowQuality();
}

nlohmann::json RenderSettings_ToJson() {
    nlohmann::json render;
    render["skyboxEnabled"] = g_renderSettings.skyboxEnabled;
    render["skyboxCubemap"] = g_renderSettings.skyboxCubemapPath;
    render["skyboxTint"] = {
        g_renderSettings.skyboxTint.x,
        g_renderSettings.skyboxTint.y,
        g_renderSettings.skyboxTint.z,
        g_renderSettings.skyboxTint.w
    };
    render["skyboxExposure"] = g_renderSettings.skyboxExposure;
    render["skyboxRotationYDeg"] = g_renderSettings.skyboxRotationYDeg;
    render["clearColor"] = {
        g_renderSettings.clearColor.x,
        g_renderSettings.clearColor.y,
        g_renderSettings.clearColor.z,
        g_renderSettings.clearColor.w
    };
    render["defaultShader"] = g_renderSettings.defaultShader;
    render["shadowQuality"] = RenderSettings_ShadowQualityId(g_renderSettings.shadowQuality);
    render["fogEnabled"] = g_renderSettings.fogEnabled;
    render["fogColor"] = {
        g_renderSettings.fogColor.x,
        g_renderSettings.fogColor.y,
        g_renderSettings.fogColor.z
    };
    render["fogDensity"] = g_renderSettings.fogDensity;
    render["fogStart"] = g_renderSettings.fogStart;
    render["fogHeight"] = g_renderSettings.fogHeight;
    render["fogHeightFalloff"] = g_renderSettings.fogHeightFalloff;
    return render;
}

void RenderSettings_ReloadSkybox() {
    Skybox_ReloadFromSettings();
}

Shader3DVariant RenderSettings_GetDefaultShader() {
    return Shader3DVariantFromId(g_renderSettings.defaultShader);
}
