/*****************************************************
 * File: engine/components/visual_component.h
 * Project: d3d11_engine
 * Author: Terry Yeung
 * -----------------------------------------------------------------------------
 * REFACTOR (2026-03-13)
 *   - Removed internal m_model / m_realModel unique_ptrs.
 *     Model loading goes through ModelAssetCache so every object shares
 *     the same GPU resource cache.
 *   - GetModel() / GetRealModel() are now const-path lookups into the cache.
 *
 * Registered with ComponentFactory as "visual".
 *
 * Fields (mirroring the JSON schema):
 *   "model"       ? primary model path (required for runtime rendering)
 *   "proxy_model" ? optional lower-poly stand-in used in editor/paused mode
 *
 * Usage:
 *   // Attach to any GameObject:
 *   auto* vc = obj->AddComponent<VisualComponent>();
 *   vc->modelPath = "rom/model/object/Enemy.fbx";
 *
 *   // Draw loop:
 *   if (auto* vc = obj->GetComponent<VisualComponent>())
 *       if (Model3D* m = vc->GetModel()) { m->SetPosition(...); m->Draw(); }
 *****************************************************/
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <DirectXMath.h>
#include "component.h"
#include "../rendering/model/model.h"
#include "../rendering/d3d11/shader.h"

class VisualComponent : public Component {
public:
    std::string modelPath;       // components.visual.model
    std::string proxyModelPath;  // components.visual.proxy_model (optional)

    bool playAnimation = false;

    // Named clip -> external animation FBX path. Empty activeClip uses base mesh anim.
    std::unordered_map<std::string, std::string> animationPaths;
    std::string activeClip;

    // Per-instance glow (shared Model3D cache must not store emission globally).
    DirectX::XMFLOAT4 emissionGlow = {0.0f, 0.0f, 0.0f, 0.0f};

    // Distance-based mesh LOD (Play / runtime only; ignored while playAnimation).
    // Level 0 is modelPath; lods[i] is level i+1. maxDistance is the camera
    // distance at which rendering switches TO that level (kept ascending).
    struct LodLevel {
        std::string modelPath;
        float maxDistance = 0.0f;
        // Screen-size mode: switch TO this level when the object's projected height drops
        // below this fraction of the viewport height (0..1). 0 = never switches.
        float screenHeight = 0.0f;
    };
    // false: levels switch at maxDistance (metres). true: at screenHeight, so the
    // switch distance follows each object's size and the camera's field of view.
    bool lodScreenSize = false;
    std::vector<LodLevel> lods;
    // Beyond this camera distance the object is not drawn. 0 = never.
    float cullDistance = 0.0f;
    // Hysteresis memory for LOD selection (not serialized).
    mutable int lodLastLevel = -1;
    // Frame serials (Lod_GetFrameSerial) stamped by GameObject::DrawVisual: last frame
    // the Visual was considered by the 3D object pass / submitted by any pass. Used to
    // skip pose evaluation while nothing draws it. Not serialized.
    mutable uint64_t lastConsideredSerial = ~uint64_t{0};
    mutable uint64_t lastDrawnSerial = ~uint64_t{0};

    // Shared-cache mesh for level (0 = modelPath). Falls back to level 0 when the
    // LOD mesh is missing or failed to load. nullptr only if level 0 is unavailable.
    Model3D* GetLodModel(int level) const;

    // Per-object shader override. Empty string uses scene default from RenderSettings.
    std::string shaderOverride;

    // SurfaceFX (shader: "surface_fx") ? JSON object "surface_fx".
    std::string surfaceFxTexturePath;
    SURFACE_FX_PARAMS surfaceFx;
    bool hasSurfaceFx = false;

    ID3D11ShaderResourceView* GetSurfaceFxSRV();

    const char* GetTypeName() const override { return "visual"; }

    void OnDetach() override;
    void OnReset() override;
    void Update(double elapsed_time) override;
    void PlayClip(const std::string& clipName);
    void PlayClip(const std::string& clipName, bool loop);
    float GetActiveClipDuration() const;
    void SetAnimationSpeed(float speed);

    // ── Model access ──────────────────────────────────────────────────────
    // Both methods look up the shared model cache.
    // They return nullptr when the path is empty or the model fails to load.

    // Editor / paused mode: prefer proxy_model, fall back to model.
    Model3D* GetModel() const {
        const std::string& path = proxyModelPath.empty() ? modelPath : proxyModelPath;
        return GetFromCache(path);
    }

    // Play mode: full-quality model; animated objects use a per-instance copy.
    Model3D* GetRealModel() const;

    std::string Serialize() const override;
    void Deserialize(const std::string& jsonStr) override;

private:
    static Model3D* GetFromCache(const std::string& path);
    void EnsureAnimInstance();
    void EnsureAnimationsLoaded();
    void ReleaseSurfaceFxTexture();

    mutable std::unique_ptr<Model3D> m_animInstance;
    std::string m_animInstancePath;   // modelPath the anim instance was built from
    bool m_animationsLoaded = false;
    std::unordered_set<std::string> m_triedAnimClips;
    float m_lastAnimSpeed = 0.0f;

    ID3D11ShaderResourceView* m_surfaceFxSRV = nullptr;
    std::string m_surfaceFxLoadedPath;
};
