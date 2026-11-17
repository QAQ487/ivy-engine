#include "engine/scene/scene_loader.h"
#include "engine/debug_draw/debug_ofstream.h"
/*****************************************************
 * File: engine/components/visual_component.cpp
 * Project: d3d11_engine
 * Author: Terry Yeung
 * Created: 2026-03-13
 *****************************************************/

#include "visual_component.h"

#include "model_asset_cache.h"
#include "engine/rendering/model/model_util.h"

#include "libraries/nlohmann/json.hpp"

namespace {

DirectX::XMFLOAT4 ReadFloat4(const nlohmann::json& j, const DirectX::XMFLOAT4& fallback) {
    if (!j.is_array() || j.size() < 3)
        return fallback;
    return DirectX::XMFLOAT4(
        j[0].get<float>(),
        j[1].get<float>(),
        j[2].get<float>(),
        j.size() >= 4 ? j[3].get<float>() : 1.0f);
}

DirectX::XMFLOAT2 ReadFloat2(const nlohmann::json& j, const DirectX::XMFLOAT2& fallback) {
    if (!j.is_array() || j.size() < 2)
        return fallback;
    return DirectX::XMFLOAT2(j[0].get<float>(), j[1].get<float>());
}

nlohmann::json ToJson(const DirectX::XMFLOAT4& v) {
    return nlohmann::json::array({ v.x, v.y, v.z, v.w });
}

nlohmann::json ToJson(const DirectX::XMFLOAT2& v) {
    return nlohmann::json::array({ v.x, v.y });
}

} // namespace

Model3D* VisualComponent::GetFromCache(const std::string& path) {
    if (path.empty()) return nullptr;
    return ModelAssetCache::Instance().GetModel(path);
}

void VisualComponent::EnsureAnimInstance() {
    if (m_animInstance || !playAnimation || modelPath.empty() || !m_animationsLoaded)
        return;

    Model3D* shared = GetFromCache(modelPath);
    if (!shared)
        return;

    m_animInstance = Model3D::CreateAnimInstanceFrom(*shared);
    if (m_animInstance)
        m_animInstancePath = modelPath;
}

Model3D* VisualComponent::GetRealModel() const {
    if (modelPath.empty())
        return nullptr;
    if (!playAnimation)
        return GetFromCache(modelPath);

    // Stale per-instance copy after modelPath change (e.g. editor mesh swap).
    if (m_animInstance && m_animationsLoaded && modelPath != m_animInstancePath) {
        auto* self = const_cast<VisualComponent*>(this);
        self->m_animInstance.reset();
        self->m_animationsLoaded = false;
        self->m_triedAnimClips.clear();
        self->m_animInstancePath.clear();
    }

    if (m_animInstance && m_animationsLoaded)
        return m_animInstance.get();

    auto* self = const_cast<VisualComponent*>(this);
    self->EnsureAnimationsLoaded();
    return m_animInstance.get();
}

void VisualComponent::ReleaseSurfaceFxTexture() {
    if (m_surfaceFxSRV) {
        m_surfaceFxSRV->Release();
        m_surfaceFxSRV = nullptr;
    }
    m_surfaceFxLoadedPath.clear();
}

ID3D11ShaderResourceView* VisualComponent::GetSurfaceFxSRV() {
    if (surfaceFxTexturePath.empty())
        return nullptr;

    if (m_surfaceFxSRV && m_surfaceFxLoadedPath == surfaceFxTexturePath)
        return m_surfaceFxSRV;

    ReleaseSurfaceFxTexture();

    const std::wstring wide = Utf8ToWide(surfaceFxTexturePath.c_str());
    ID3D11ShaderResourceView* srv = nullptr;
    if (LoadTextureWIC_SRV(wide.c_str(), &srv) && srv) {
        m_surfaceFxSRV = srv;
        m_surfaceFxLoadedPath = surfaceFxTexturePath;
    }
    return m_surfaceFxSRV;
}

void VisualComponent::OnDetach() {
    m_animInstance.reset();
    m_animInstancePath.clear();
    ReleaseSurfaceFxTexture();
}

void VisualComponent::OnReset() {
    m_animInstance.reset();
    m_animInstancePath.clear();
    m_animationsLoaded = false;
    m_triedAnimClips.clear();
    ReleaseSurfaceFxTexture();
}

void VisualComponent::EnsureAnimationsLoaded() {
    Model3D* shared = GetFromCache(modelPath);
    if (!shared) return;

    bool addedAny = false;
    for (const auto& [name, path] : animationPaths) {
        if (name.empty() || path.empty())
            continue;
        if (shared->HasAnimation(name))
            continue;
        // A missing FBX used to retry Import every frame and rebuild the
        // skinned instance ? that cut FPS in half after adding pickup.
        if (!m_triedAnimClips.insert(name).second)
            continue;
        shared->EnsureAnimation(name, path.c_str());
        if (shared->HasAnimation(name))
            addedAny = true;
    }

    // Anim-instance m_animScenes is a snapshot of shared at create time.
    // Rebuild when this component registers new clips, or when shared gained
    // a clip (from another VisualComponent) that this instance still lacks.
    bool instanceMissingClip = false;
    if (m_animInstance) {
        for (const auto& [name, path] : animationPaths) {
            if (name.empty())
                continue;
            if (shared->HasAnimation(name) && !m_animInstance->HasAnimation(name)) {
                instanceMissingClip = true;
                break;
            }
        }
    }

    // Another component may have loaded clips onto the shared asset after this
    // instance was created. Merge without a full instance rebuild (BakeSkinning).
    if (instanceMissingClip && m_animInstance && m_animationsLoaded && !addedAny) {
        m_animInstance->MergeAnimScenesFrom(*shared);
        instanceMissingClip = false;
    }

    const bool needInstance =
        playAnimation && (!m_animationsLoaded || addedAny || instanceMissingClip);
    if (needInstance) {
        const std::string clipToRestore = activeClip;
        m_animInstance.reset();
        m_animationsLoaded = true;
        EnsureAnimInstance();
        if (Model3D* playback = m_animInstance.get()) {
            if (!clipToRestore.empty() && playback->HasAnimation(clipToRestore))
                playback->PlayAnimation(clipToRestore);
            else
                playback->PlayAnimation("");
        }
        return;
    }

    if (!m_animationsLoaded) {
        m_animationsLoaded = true;
        Model3D* playback = GetRealModel();
        if (!playback) return;
        if (!activeClip.empty() && playback->HasAnimation(activeClip))
            PlayClip(activeClip);
    }
}

void VisualComponent::PlayClip(const std::string& clipName) {
    PlayClip(clipName, true);
}

void VisualComponent::PlayClip(const std::string& clipName, bool loop) {
    EnsureAnimationsLoaded();

    const std::string resolvedClip =
        (clipName.empty() || clipName == "base") ? std::string() : clipName;
    activeClip = resolvedClip;

    Model3D* model = GetRealModel();
    if (!model) return;

    model->SetLoop(loop);

    const bool hasAnim = resolvedClip.empty() ? true : model->HasAnimation(resolvedClip);
    if (model->GetActiveClip() == resolvedClip)
        return;

    m_lastAnimSpeed = 0.0f;

    if (resolvedClip.empty())
        model->PlayAnimation("");
    else if (hasAnim)
        model->PlayAnimation(resolvedClip);

    model->SetLoop(loop);
}

float VisualComponent::GetActiveClipDuration() const {
    Model3D* model = GetRealModel();
    if (!model)
        return 0.0f;
    return static_cast<float>(model->GetAnimDurationSec());
}

void VisualComponent::SetAnimationSpeed(float speed) {
    if (speed == m_lastAnimSpeed)
        return;
    m_lastAnimSpeed = speed;

    Model3D* model = GetRealModel();
    if (!model)
        return;
    model->SetAnimationSpeed(static_cast<double>(speed));
}

void VisualComponent::Update(double elapsed_time) {
    if(SceneLoader_Is2D()) return;
    if (!playAnimation) return;

    if (!m_animInstance || !m_animationsLoaded)
        EnsureAnimationsLoaded();

    Model3D* model = GetRealModel();
    if (!model) return;

    model->UpdateAnimation(elapsed_time);
}

std::string VisualComponent::Serialize() const {
    nlohmann::json j;
    j["model"] = modelPath;
    if (!proxyModelPath.empty())
        j["proxy_model"] = proxyModelPath;
    if (playAnimation)
        j["animate"] = true;
    if (!activeClip.empty())
        j["active_clip"] = activeClip;
    if (!animationPaths.empty())
        j["animations"] = animationPaths;
    if (!shaderOverride.empty())
        j["shader"] = shaderOverride;
    if (hasSurfaceFx) {
        nlohmann::json sfx;
        if (!surfaceFxTexturePath.empty())
            sfx["texture"] = surfaceFxTexturePath;
        sfx["color1"] = ToJson(surfaceFx.Color1);
        sfx["color2"] = ToJson(surfaceFx.Color2);
        sfx["scroll_speed1"] = ToJson(surfaceFx.ScrollSpeed1);
        sfx["scroll_speed2"] = ToJson(surfaceFx.ScrollSpeed2);
        sfx["uv_scale"] = surfaceFx.UvScale;
        sfx["fresnel_power"] = surfaceFx.FresnelPower;
        sfx["emission_strength"] = surfaceFx.EmissionStrength;
        sfx["gradient_start"] = surfaceFx.GradientStart;
        sfx["gradient_end"] = surfaceFx.GradientEnd;
        sfx["edge_fade_start"] = surfaceFx.EdgeFadeStart;
        sfx["edge_fade_end"] = surfaceFx.EdgeFadeEnd;
        j["surface_fx"] = sfx;
    }
    return j.dump();
}

void VisualComponent::Deserialize(const std::string& jsonStr) {
    try {
        auto j = nlohmann::json::parse(jsonStr);

        modelPath      = j.value("model",       "");
        proxyModelPath = j.value("proxy_model", "");
        playAnimation  = j.value("animate",     false);
        activeClip     = j.value("active_clip", "");
        shaderOverride = j.value("shader", "");

        animationPaths.clear();
        if (j.contains("animations") && j["animations"].is_object()) {
            for (auto& [key, val] : j["animations"].items()) {
                if (val.is_string())
                    animationPaths[key] = val.get<std::string>();
            }
        }

        hasSurfaceFx = false;
        surfaceFxTexturePath.clear();
        surfaceFx = SURFACE_FX_PARAMS{};
        ReleaseSurfaceFxTexture();

        if (j.contains("surface_fx") && j["surface_fx"].is_object()) {
            const auto& sfx = j["surface_fx"];
            hasSurfaceFx = true;
            surfaceFxTexturePath = sfx.value("texture", "");
            if (sfx.contains("color1"))
                surfaceFx.Color1 = ReadFloat4(sfx["color1"], surfaceFx.Color1);
            if (sfx.contains("color2"))
                surfaceFx.Color2 = ReadFloat4(sfx["color2"], surfaceFx.Color2);
            if (sfx.contains("scroll_speed1"))
                surfaceFx.ScrollSpeed1 = ReadFloat2(sfx["scroll_speed1"], surfaceFx.ScrollSpeed1);
            if (sfx.contains("scroll_speed2"))
                surfaceFx.ScrollSpeed2 = ReadFloat2(sfx["scroll_speed2"], surfaceFx.ScrollSpeed2);
            surfaceFx.UvScale = sfx.value("uv_scale", surfaceFx.UvScale);
            surfaceFx.FresnelPower = sfx.value("fresnel_power", surfaceFx.FresnelPower);
            surfaceFx.EmissionStrength = sfx.value("emission_strength", surfaceFx.EmissionStrength);
            surfaceFx.GradientStart = sfx.value("gradient_start", surfaceFx.GradientStart);
            surfaceFx.GradientEnd = sfx.value("gradient_end", surfaceFx.GradientEnd);
            surfaceFx.EdgeFadeStart = sfx.value("edge_fade_start", surfaceFx.EdgeFadeStart);
            surfaceFx.EdgeFadeEnd = sfx.value("edge_fade_end", surfaceFx.EdgeFadeEnd);
        }

        if (shaderOverride == "surface_fx")
            hasSurfaceFx = true;

        m_animationsLoaded = false;
        m_triedAnimClips.clear();
        m_animInstance.reset();
    } catch (...) {
        hal::DebugWriteA("[VisualComponent] Deserialize: JSON parse error.\n");
    }
}
