// effect_component.cpp
// ECS: effect component implementation.

#include "engine/scene/scene_loader.h"
#include "effect_component.h"
#include "effect_system_flush.h"

#include "engine/ecs/game_object.h"
#include "engine/ecs/transform.h"
#include "engine/rendering/effect/effect_system.h"

#include "libraries/nlohmann/json.hpp"

#include <Windows.h>
#include <string>

using json = nlohmann::json;

namespace {

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) return {};

    int required = MultiByteToWideChar(
        CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (required <= 0) {
        required = MultiByteToWideChar(
            CP_ACP, 0, value.c_str(), -1, nullptr, 0);
        if (required <= 0) return {};

        std::wstring wide(static_cast<size_t>(required), L'\0');
        MultiByteToWideChar(
            CP_ACP, 0, value.c_str(), -1, wide.data(), required);
        if (!wide.empty() && wide.back() == L'\0') wide.pop_back();
        return wide;
    }

    std::wstring wide(static_cast<size_t>(required), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, value.c_str(), -1, wide.data(), required);
    if (!wide.empty() && wide.back() == L'\0') wide.pop_back();
    return wide;
}

} // namespace

void EffectComponent::OnAttach() {
    EffectSystem_RegisterComponent(this);
    LoadEffect();
}

void EffectComponent::OnDetach() {
    EffectSystem_UnregisterComponent(this);
    ApplyStopImmediate();
    m_cmdPlay = m_cmdStop = m_cmdSync = m_cmdCheckExists = false;
    m_pendingAutoDestroy = false;
}

void EffectComponent::Update(double /*elapsed_time*/) {
    if(SceneLoader_Is2D()) return;
    if (!owner || !owner->IsActive()) return;

    m_cmdCheckExists = true;

    if (autoPlay && !m_hasAutoPlayed) {
        Play();
        m_hasAutoPlayed = true;
    }

    if (m_handle >= 0 || m_cmdPlay)
        m_cmdSync = true;
}

void EffectComponent::OnReset() {
    Stop();
    m_hasAutoPlayed = false;
    LoadEffect();
}

std::string EffectComponent::Serialize() const {
    json j;
    j["effectPath"] = effectPath;
    j["autoPlay"] = autoPlay;
    j["autoDestroy"] = autoDestroy;
    j["speed"] = speed;
    j["lifetimeOverride"] = lifetimeOverride;
    j["randomStartOffset"] = randomStartOffset;
    return j.dump();
}

void EffectComponent::Deserialize(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        effectPath = j.value("effectPath", "");
        autoPlay = j.value("autoPlay", true);
        autoDestroy = j.value("autoDestroy", false);
        speed = j.value("speed", 1.0f);
        lifetimeOverride = j.value("lifetimeOverride", -1.0f);
        randomStartOffset = j.value("randomStartOffset", 0.0f);

        Stop();
        m_effectId = -1;
        m_hasAutoPlayed = false;
    } catch (...) {
        OutputDebugStringA("[EffectComponent] Deserialize failed.\n");
    }
}

void EffectComponent::Play() {
    m_cmdStop = true;
    m_cmdPlay = true;
    if (!GameObject::IsInsideUpdate())
        FlushCommands();
}

void EffectComponent::Stop() {
    m_cmdPlay = false;
    m_cmdStop = true;
    if (!GameObject::IsInsideUpdate())
        FlushCommands();
}

bool EffectComponent::IsPlaying() const {
    if (m_cmdPlay)
        return true;
    return m_handle >= 0 && EffectSystem::Exists(m_handle);
}

void EffectComponent::SetEffectPath(const std::string& path) {
    if (effectPath == path) return;

    Stop();
    effectPath = path;
    m_effectId = -1;
    m_hasAutoPlayed = false;
    LoadEffect();
}

void EffectComponent::ApplyStopImmediate() {
    if (m_handle >= 0) {
        EffectSystem::Stop(m_handle);
        m_handle = -1;
    }
}

void EffectComponent::FlushCommands() {
    if (m_cmdStop) {
        ApplyStopImmediate();
        m_cmdStop = false;
    }

    if (m_cmdCheckExists) {
        m_cmdCheckExists = false;
        if (m_handle >= 0 && !EffectSystem::Exists(m_handle)) {
            m_handle = -1;
            if (autoDestroy)
                m_pendingAutoDestroy = true;
        }
    }

    if (m_cmdPlay) {
        m_cmdPlay = false;
        if (m_effectId < 0)
            LoadEffect();
        if (m_effectId >= 0 && owner) {
            ApplyStopImmediate();
            const DirectX::XMFLOAT3 pos = owner->GetPosition();
            m_handle = EffectSystem::Play(m_effectId, pos);
            if (m_handle >= 0) {
                SyncTransformImmediate();
                EffectSystem::SetSpeed(m_handle, speed);
            }
        }
    }

    if (m_cmdSync) {
        m_cmdSync = false;
        if (m_handle >= 0) {
            SyncTransformImmediate();
            EffectSystem::SetSpeed(m_handle, speed);
        }
    }

    if (m_pendingAutoDestroy) {
        m_pendingAutoDestroy = false;
        if (owner)
            owner->SetActive(false);
    }
}

bool EffectComponent::LoadEffect() {
    if (m_effectId >= 0) return true;
    if (effectPath.empty()) return false;

    const std::wstring widePath = Utf8ToWide(effectPath);
    if (widePath.empty()) return false;

    m_effectId = EffectSystem::LoadCached(widePath.c_str());
    if (m_effectId < 0) {
        OutputDebugStringA(("[EffectComponent] Failed to load effect: "
            + effectPath + "\n").c_str());
        return false;
    }
    return true;
}

void EffectComponent::SyncTransformImmediate() {
    if (!owner || m_handle < 0) return;

    DirectX::XMFLOAT3 worldPos, worldRot, worldScale;
    if (const engine::Transform* t = owner->GetTransform()) {
        t->GetWorldPositionRotationScale(worldPos, worldRot, worldScale);
    } else {
        worldPos = owner->GetPosition();
        worldRot = owner->GetRotation();
        worldScale = owner->GetScale();
    }

    EffectSystem::SetLocation(m_handle, worldPos);
    EffectSystem::SetRotationEulerDeg(m_handle, worldRot);
    EffectSystem::SetScale(m_handle, worldScale);
}
