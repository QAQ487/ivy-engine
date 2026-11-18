/**===========================================================================
 File   :  [effect_system.cpp]
 Author : Terry Yeung
 Date   : 2025/10/24
-----------------------------------------------------------------------------
===========================================================================**/
#include "effect_system.h"

#include <Windows.h>

#include <vector>
#include <memory>
#include <cassert>
#include <cstring>
#include <string>
#include <map>
#include <unordered_map>

#include "engine/rendering/d3d11/renderer.h" // GetDevice/GetDeviceContext, GetBackBufferWidth/Height
#include "engine/core/content_path.h"
#include "camera.h"   // GetCamera()
#include "engine/rendering/culling/frustum_cull_math.h"
#include "engine/rendering/culling/fx_cull.h"
#include "engine/rendering/culling/fx_cull_math.h"
#include "engine/scene/render_settings.h"
#include "engine/debug_draw/debug_ofstream.h"

using namespace DirectX;

namespace {
    ::Effekseer::ManagerRef                    g_manager;
    ::EffekseerRendererDX11::RendererRef       g_renderer;
    ::Effekseer::Backend::GraphicsDeviceRef    g_gfxDevice;

    // Bounds bookkeeping for effect culling. Effekseer handles are the engine's only
    // identity for a playing effect; position and scale come from Play / SetLocation /
    // SetScale. std::map keeps iteration deterministic.
    struct TrackedEffect {
        DirectX::XMFLOAT3 position { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 scale { 1.0f, 1.0f, 1.0f };
        bool drawn = true;   // current Effekseer auto-drawing state
    };
    std::map<int, TrackedEffect> g_tracked;

    std::vector<::Effekseer::EffectRef> g_effects;
    std::unordered_map<std::wstring, int> g_effectCache;
    float g_timeSec = 0.0f;

    inline ::Effekseer::Matrix44 MakeProjectionLH(float fovRad, float aspect, float zn, float zf) {
        ::Effekseer::Matrix44 m {};
        m.PerspectiveFovLH(fovRad, aspect, zn, zf);
        return m;
    }
    inline ::Effekseer::Matrix44 MakeViewLookAtLH(const XMFLOAT3& eye, const XMFLOAT3& at, const XMFLOAT3& up) {
        ::Effekseer::Matrix44 m {};
        m.LookAtLH(
            ::Effekseer::Vector3D(eye.x, eye.y, eye.z),
            ::Effekseer::Vector3D(at.x, at.y, at.z),
            ::Effekseer::Vector3D(up.x, up.y, up.z)
        );
        return m;
    }
} // anonymous

namespace EffectSystem {
    bool Initialize() {
        if (g_manager != nullptr) return true;

        auto* dev = GetDevice();
        auto* ctx = GetDeviceContext();
        if (dev == nullptr || ctx == nullptr) return false;

        g_gfxDevice = ::EffekseerRendererDX11::CreateGraphicsDevice(dev, ctx);
        if (g_gfxDevice == nullptr) return false;

        g_renderer = ::EffekseerRendererDX11::Renderer::Create(g_gfxDevice, 8000);
        if (g_renderer == nullptr) return false;

        g_manager = ::Effekseer::Manager::Create(8000);
        if (g_manager == nullptr) return false;

        g_manager->SetSpriteRenderer(g_renderer->CreateSpriteRenderer());
        g_manager->SetRibbonRenderer(g_renderer->CreateRibbonRenderer());
        g_manager->SetRingRenderer(g_renderer->CreateRingRenderer());
        g_manager->SetTrackRenderer(g_renderer->CreateTrackRenderer());
        g_manager->SetModelRenderer(g_renderer->CreateModelRenderer());

        g_manager->SetTextureLoader(g_renderer->CreateTextureLoader());
        g_manager->SetModelLoader(g_renderer->CreateModelLoader());
        g_manager->SetMaterialLoader(g_renderer->CreateMaterialLoader());
        g_manager->SetCurveLoader(Effekseer::MakeRefPtr<Effekseer::CurveLoader>());

        // Left-handed coordinates
        g_manager->SetCoordinateSystem(::Effekseer::CoordinateSystem::LH);

        // Enable D3D state restoration
        g_renderer->SetRestorationOfStatesFlag(true);

        g_effects.clear();
        g_timeSec = 0.0f;

        return true;
    }

    void Finalize() {
        g_tracked.clear();
        g_effectCache.clear();
        g_effects.clear();

        if (g_manager != nullptr) { g_manager.Reset(); }
        if (g_renderer != nullptr) { g_renderer.Reset(); }
        if (g_gfxDevice != nullptr) { g_gfxDevice.Reset(); }
    }

    void Update(double deltaSeconds) {
        if (g_manager == nullptr) return;
        g_timeSec += static_cast<float>(deltaSeconds);
        ::Effekseer::Manager::UpdateParameter param;
        g_manager->Update(param);
    }

    void Draw3D() {
        if (g_manager == nullptr || g_renderer == nullptr) return;

        g_renderer->SetTime(g_timeSec);

        const auto& cam = GetCamera();
        const float width = GetRenderViewportWidth();
        const float height = GetRenderViewportHeight();
        const float aspect = (height > 0.0f) ? (width / height) : 1.0f;

        const float fovRad = XMConvertToRadians(cam.fov);
        ::Effekseer::Matrix44 efkProj = MakeProjectionLH(fovRad, aspect, cam.nearClip, cam.farClip);
        ::Effekseer::Matrix44 efkView = MakeViewLookAtLH(cam.position, cam.at, cam.up);

        g_renderer->SetProjectionMatrix(efkProj);
        g_renderer->SetCameraMatrix(efkView);

        ::Effekseer::Manager::LayerParameter layer {};
        layer.ViewerPosition = ::Effekseer::Vector3D(cam.position.x, cam.position.y, cam.position.z);
        g_manager->SetLayerParameter(0, layer);

        // Per-effect frustum cull through Effekseer's auto-drawing flag, so Manager::Draw
        // keeps its own draw ordering. Culled effects still simulate.
        {
            const auto& rs = RenderSettings_GetConst();
            const bool cull = rs.effectCullingEnabled;
            frustum_cull::FrustumPlanes planes;
            if (cull) {
                frustum_cull::ExtractFrustumPlanesD3D(
                    cam.GetViewMatrix() * cam.GetProjMatrix(width, height), planes);
            }
            int considered = 0;
            int culled = 0;
            for (auto it = g_tracked.begin(); it != g_tracked.end();) {
                if (!g_manager->Exists(it->first)) {
                    it = g_tracked.erase(it);
                    continue;
                }
                TrackedEffect& t = it->second;
                bool visible = true;
                if (cull) {
                    ++considered;
                    visible = fx_cull::SphereIntersectsFrustum(
                        t.position.x, t.position.y, t.position.z,
                        fx_cull::EffectRadius(rs.effectCullRadius, t.scale.x, t.scale.y, t.scale.z),
                        planes);
                    if (!visible) ++culled;
                }
                if (visible != t.drawn) {
                    g_manager->SetAutoDrawing(it->first, visible);
                    t.drawn = visible;
                }
                ++it;
            }
            if (cull)
                FxCull_RecordEffects(considered, culled);
        }

        g_renderer->BeginRendering();

        ::Effekseer::Manager::DrawParameter dp {};
        dp.ZNear = cam.nearClip;
        dp.ZFar = cam.farClip;
        dp.ViewProjectionMatrix = g_renderer->GetCameraProjectionMatrix();

        g_manager->Draw(dp);

        g_renderer->EndRendering();
    }

    int Load(const char16_t* path) {
        if (g_manager == nullptr || path == nullptr || *path == u'\0') return -1;

        auto eff = ::Effekseer::Effect::Create(g_manager, path);
        if (eff == nullptr) return -1;

        g_effects.emplace_back(eff);
        return static_cast<int>(g_effects.size() - 1);
    }

    int LoadW(const wchar_t* path) {
        if (path == nullptr || *path == L'\0') return -1;
        const std::wstring resolved = content_path::ReadableWideCurrent(path);
        const wchar_t* openPath = resolved.empty() ? path : resolved.c_str();
        return Load(reinterpret_cast<const char16_t*>(openPath));
    }

    bool ReloadContent() {
        if (g_manager == nullptr) return true;
        g_manager->GetSetting()->SetIsFileCacheEnabled(false);
        std::vector<std::pair<int, ::Effekseer::EffectRef>> candidates;
        for (const auto& entry : g_effectCache) {
            const std::wstring resolved = content_path::ReadableWideCurrent(entry.first.c_str());
            const wchar_t* openPath = resolved.empty() ? entry.first.c_str() : resolved.c_str();
            auto effect = ::Effekseer::Effect::Create(g_manager, reinterpret_cast<const char16_t*>(openPath));
            if (effect == nullptr) { g_manager->GetSetting()->SetIsFileCacheEnabled(true); return false; }
            candidates.emplace_back(entry.second, effect);
        }
        StopAll();
        for (const auto& entry : candidates) g_effects[entry.first] = entry.second;
        g_manager->GetSetting()->SetIsFileCacheEnabled(true);
        return true;
    }
    int LoadCached(const wchar_t* path) {
        if (path == nullptr || *path == L'\0') {
            hal::DebugWriteW(L"[EffectSystem] LoadCached invalid path.\n");
            return -1;
        }

        std::wstring key(path);
        auto it = g_effectCache.find(key);
        if (it != g_effectCache.end()) {
            hal::DebugWriteW((L"[EffectSystem] LoadCached hit: id="
                + std::to_wstring(it->second) + L" path=" + key + L"\n").c_str());
            return it->second;
        }

        hal::DebugWriteW((L"[EffectSystem] LoadCached miss: " + key + L"\n").c_str());

        int id = LoadW(path);
        if (id >= 0) {
            g_effectCache[key] = id;
            hal::DebugWriteW((L"[EffectSystem] LoadCached stored: id="
                + std::to_wstring(id) + L" path=" + key + L"\n").c_str());
        } else {
            hal::DebugWriteW((L"[EffectSystem] LoadCached failed: " + key + L"\n").c_str());
        }
        return id;
    }

    int Play(int effectId, const DirectX::XMFLOAT3& pos) {
        if (g_manager == nullptr) return -1;
        if (effectId < 0 || static_cast<size_t>(effectId) >= g_effects.size()) return -1;
        if (g_effects[effectId] == nullptr) return -1;

        auto h = g_manager->Play(g_effects[effectId], pos.x, pos.y, pos.z);
        if (h < 0) return -1;
        TrackedEffect t;
        t.position = pos;
        g_tracked[h] = t;
        return h;
    }

    void Stop(int handle) {
        if (g_manager == nullptr) return;
        if (handle >= 0) g_manager->StopEffect(handle);
    }

    void StopAll() {
        if (g_manager == nullptr) return;
        g_manager->StopAllEffects();
        g_tracked.clear();
    }

    bool Exists(int handle) {
        if (g_manager == nullptr) return false;
        return g_manager->Exists(handle);
    }

    // Transform / playback control
    void SetLocation(int handle, const DirectX::XMFLOAT3& pos) {
        if (g_manager == nullptr || handle < 0) return;
        g_manager->SetLocation(handle, pos.x, pos.y, pos.z);
        const auto it = g_tracked.find(handle);
        if (it != g_tracked.end()) it->second.position = pos;
    }

    void SetRotationEulerDeg(int handle, const DirectX::XMFLOAT3& rotDeg) {
        if (g_manager == nullptr || handle < 0) return;
        const float rx = XMConvertToRadians(rotDeg.x);
        const float ry = XMConvertToRadians(rotDeg.y);
        const float rz = XMConvertToRadians(rotDeg.z);
        g_manager->SetRotation(handle, rx, ry, rz);
    }

    void SetScale(int handle, const DirectX::XMFLOAT3& scale) {
        if (g_manager == nullptr || handle < 0) return;
        g_manager->SetScale(handle, scale.x, scale.y, scale.z);
        const auto it = g_tracked.find(handle);
        if (it != g_tracked.end()) it->second.scale = scale;
    }

    void SetScale(int handle, float uniform) {
        if (g_manager == nullptr || handle < 0) return;
        g_manager->SetScale(handle, uniform, uniform, uniform);
        const auto it = g_tracked.find(handle);
        if (it != g_tracked.end()) it->second.scale = { uniform, uniform, uniform };
    }

    void SetSpeed(int handle, float speed) {
        if (g_manager == nullptr || handle < 0) return;
        g_manager->SetSpeed(handle, speed);
    }

    void SetPaused(int handle, bool paused) {
        if (g_manager == nullptr || handle < 0) return;
        g_manager->SetPaused(handle, paused);
    }

    void SetShown(int handle, bool shown) {
        if (g_manager == nullptr || handle < 0) return;
        g_manager->SetShown(handle, shown);
    }
}