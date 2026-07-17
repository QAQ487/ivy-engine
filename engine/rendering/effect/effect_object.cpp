/**===========================================================================
 File   :  [effect_object.cpp]
 Author : Terry
 Date   : 2025/10/24
-----------------------------------------------------------------------------
===========================================================================**/
#include "effect_object.h"
#include "scene_manager.h"

using namespace DirectX;

bool EffectObject::LoadFromFileW(const wchar_t* path) {
    if (!path || !*path) return false;
    m_effectId = EffectSystem::LoadW(path);
    return (m_effectId >= 0);
}

EffectObject& EffectObject::WithScale(float uniform) {
    GameObject::SetScale({uniform, uniform, uniform});
    if (m_handle >= 0) {
        // 一様スケールサポート（EffectSystem に一様版がある場合そちらを優先）
        EffectSystem::SetScale(m_handle, XMFLOAT3 {uniform, uniform, uniform});
    }
    return *this;
}

EffectObject& EffectObject::AttachToCurrentScene() {
    if (auto* scene = SceneManager::Instance().GetCurrentScene()) {
        scene->AddObject(this);
    }
    return *this;
}

EffectObject& EffectObject::InitNow() {
    Initialize();
    return *this;
}

void EffectObject::Initialize() {
    if (m_autoPlay && m_effectId >= 0 && m_handle < 0) {
        m_handle = EffectSystem::Play(m_effectId, m_position);
        if (m_handle >= 0) {
            // 初期Transformを反映
            EffectSystem::SetLocation(m_handle, m_position);
            EffectSystem::SetRotationEulerDeg(m_handle, m_rotation);
            EffectSystem::SetScale(m_handle, m_scale);
        }
    }
}

void EffectObject::Finalize() {
    if (m_handle >= 0) {
        EffectSystem::Stop(m_handle);
        m_handle = -1;
    }
}

void EffectObject::LogicUpdate(double /*elapsed_time*/) {
    if (m_handle >= 0) {
        // GameObject の Transform をエフェクトへ同期
        EffectSystem::SetLocation(m_handle, m_position);
        EffectSystem::SetRotationEulerDeg(m_handle, m_rotation);
        EffectSystem::SetScale(m_handle, m_scale);

        // 1ショット終了時に自動非アクティブ化
        if (m_autoDestroyOnEnd && !EffectSystem::Exists(m_handle)) {
            m_handle = -1;
            m_isActive = false;
        }
    }
    else {
        // 自動再生ONで未再生かつ有効なら再生を試みる
        if (m_autoPlay && m_effectId >= 0 && m_isActive) {
            m_handle = EffectSystem::Play(m_effectId, m_position);
            if (m_handle >= 0) {
                EffectSystem::SetLocation(m_handle, m_position);
                EffectSystem::SetRotationEulerDeg(m_handle, m_rotation);
                EffectSystem::SetScale(m_handle, m_scale);
            }
        }
    }
}

void EffectObject::Draw() {
    // Effekseer の描画は EffectSystem::Draw3D() 側でまとめて実施
}

void EffectObject::Play() {
    if (m_effectId < 0) return;
    if (m_handle >= 0) EffectSystem::Stop(m_handle);
    m_handle = EffectSystem::Play(m_effectId, m_position);
    if (m_handle >= 0) {
        EffectSystem::SetLocation(m_handle, m_position);
        EffectSystem::SetRotationEulerDeg(m_handle, m_rotation);
        EffectSystem::SetScale(m_handle, m_scale);
    }
}

void EffectObject::Stop() {
    if (m_handle >= 0) {
        EffectSystem::Stop(m_handle);
        m_handle = -1;
    }
}

void EffectObject::SetSpeed(float speed) {
    if (m_handle >= 0) EffectSystem::SetSpeed(m_handle, speed);
}
void EffectObject::SetPaused(bool paused) {
    if (m_handle >= 0) EffectSystem::SetPaused(m_handle, paused);
}
void EffectObject::SetShown(bool shown) {
    if (m_handle >= 0) EffectSystem::SetShown(m_handle, shown);
}

EffectObject* EffectObject::SpawnW(const wchar_t* path,
    const XMFLOAT3& pos,
    bool autoDestroyOnEnd,
    const XMFLOAT3& rotDeg,
    const XMFLOAT3& scale) {
    auto* obj = new EffectObject();
    obj->SetPosition(pos);
    obj->SetRotation(rotDeg);
    obj->SetScale(scale);
    obj->SetAutoDestroyOnEnd(autoDestroyOnEnd);
    obj->SetAutoPlay(true);

    if (!obj->LoadFromFileW(path)) {
        delete obj;
        return nullptr;
    }
    if (auto* scene = SceneManager::Instance().GetCurrentScene()) {
        scene->AddObject(obj);
    }
    obj->Initialize();
    return obj;
}

void Effect_Initialize() { 

}

void Effect_Finalize() {
}

void Effect_Update(double elapsed_time) {
}

void Effect_Draw() {
}
