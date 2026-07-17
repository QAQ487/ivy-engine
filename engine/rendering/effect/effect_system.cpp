/**===========================================================================
 File   :  [effect_system.cpp]
 Author : Terry
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
#include <unordered_map>

#include "engine/rendering/d3d11/renderer.h" // GetDevice/GetDeviceContext, GetBackBufferWidth/Height
#include "camera.h"   // GetCamera()

using namespace DirectX;

namespace {
    ::Effekseer::ManagerRef                    g_manager;
    ::EffekseerRendererDX11::RendererRef       g_renderer;
    ::Effekseer::Backend::GraphicsDeviceRef    g_gfxDevice;

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

        // 左手座標
        g_manager->SetCoordinateSystem(::Effekseer::CoordinateSystem::LH);

        // D3Dの状態復元を有効化
        g_renderer->SetRestorationOfStatesFlag(true);

        g_effects.clear();
        g_timeSec = 0.0f;

        return true;
    }

    void Finalize() {
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
        return Load(reinterpret_cast<const char16_t*>(path));
    }

    int LoadCached(const wchar_t* path) {
        if (path == nullptr || *path == L'\0') {
            OutputDebugStringW(L"[EffectSystem] LoadCached invalid path.\n");
            return -1;
        }

        std::wstring key(path);
        auto it = g_effectCache.find(key);
        if (it != g_effectCache.end()) {
            OutputDebugStringW((L"[EffectSystem] LoadCached hit: id="
                + std::to_wstring(it->second) + L" path=" + key + L"\n").c_str());
            return it->second;
        }

        OutputDebugStringW((L"[EffectSystem] LoadCached miss: " + key + L"\n").c_str());

        int id = LoadW(path);
        if (id >= 0) {
            g_effectCache[key] = id;
            OutputDebugStringW((L"[EffectSystem] LoadCached stored: id="
                + std::to_wstring(id) + L" path=" + key + L"\n").c_str());
        } else {
            OutputDebugStringW((L"[EffectSystem] LoadCached failed: " + key + L"\n").c_str());
        }
        return id;
    }

    int Play(int effectId, const DirectX::XMFLOAT3& pos) {
        if (g_manager == nullptr) return -1;
        if (effectId < 0 || static_cast<size_t>(effectId) >= g_effects.size()) return -1;
        if (g_effects[effectId] == nullptr) return -1;

        auto h = g_manager->Play(g_effects[effectId], pos.x, pos.y, pos.z);
        return h >= 0 ? h : -1;
    }

    void Stop(int handle) {
        if (g_manager == nullptr) return;
        if (handle >= 0) g_manager->StopEffect(handle);
    }

    void StopAll() {
        if (g_manager == nullptr) return;
        g_manager->StopAllEffects();
    }

    bool Exists(int handle) {
        if (g_manager == nullptr) return false;
        return g_manager->Exists(handle);
    }

    // Transform / 再生制御
    void SetLocation(int handle, const DirectX::XMFLOAT3& pos) {
        if (g_manager == nullptr || handle < 0) return;
        g_manager->SetLocation(handle, pos.x, pos.y, pos.z);
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
    }

    void SetScale(int handle, float uniform) {
        if (g_manager == nullptr || handle < 0) return;
        g_manager->SetScale(handle, uniform, uniform, uniform);
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