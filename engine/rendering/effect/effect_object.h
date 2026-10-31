/**===========================================================================
 File   :  [effect_object.h]
 Author : Terry Yeung
 Date   : 2025/10/24
-----------------------------------------------------------------------------
===========================================================================**/
#ifndef EFFECT_OBJECT_H
#define EFFECT_OBJECT_H

#include "engine/ecs/game_object.h"
#include "effect_system.h"

class EffectObject : public GameObject {
public:
    EffectObject() = default;
    explicit EffectObject(int effectId, bool autoPlay = true) { m_effectId = effectId; m_autoPlay = autoPlay; }
    explicit EffectObject(const wchar_t* path, bool autoPlay = true) { LoadFromFileW(path); m_autoPlay = autoPlay; }
    ~EffectObject() override { Finalize(); }

    // 1ショット終了で自動的に非アクティブ化するか
    void SetAutoDestroyOnEnd(bool enable) { m_autoDestroyOnEnd = enable; }
    bool GetAutoDestroyOnEnd() const { return m_autoDestroyOnEnd; }

    // 自動再生するか（Initialize 時）
    void SetAutoPlay(bool enable) { m_autoPlay = enable; }
    bool GetAutoPlay() const { return m_autoPlay; }

    // 速度・一時停止・表示制御
    void SetSpeed(float speed);
    void SetPaused(bool paused);
    void SetShown(bool shown);

    // 明示的に再生・停止
    void Play();
    void Stop();

    // 読み込み（.efk / .efkefc）
    // 成功で true。すでに読み込み済みなら上書き。
    bool LoadFromFileW(const wchar_t* path);
    // 既に Load 済みの effectId を直接指定する場合
    void SetEffectId(int effectId) { m_effectId = effectId; }

    // 使いやすさ向上: チェーン可能なビルダー風API
    EffectObject& WithEffectId(int effectId) { m_effectId = effectId; return *this; }
    EffectObject& WithPosition(const DirectX::XMFLOAT3& pos) { GameObject::SetPosition(pos); if (m_handle >= 0) EffectSystem::SetLocation(m_handle, pos); return *this; }
    EffectObject& WithRotation(const DirectX::XMFLOAT3& rotDeg) { GameObject::SetRotation(rotDeg); if (m_handle >= 0) EffectSystem::SetRotationEulerDeg(m_handle, rotDeg); return *this; }
    EffectObject& WithScale(const DirectX::XMFLOAT3& s) { GameObject::SetScale(s); if (m_handle >= 0) EffectSystem::SetScale(m_handle, s); return *this; }
    EffectObject& WithScale(float uniform); // 一様スケール版（実装はcpp）
    EffectObject& WithAutoPlay(bool e) { m_autoPlay = e; return *this; }
    EffectObject& WithAutoDestroyOnEnd(bool e) { m_autoDestroyOnEnd = e; return *this; }

    // シーン登録＆初期化＆制御のチェーンAPI
    EffectObject& AttachToCurrentScene();
    EffectObject& InitNow();
    EffectObject& PlayNow() { Play(); return *this; }
    EffectObject& StopNow() { Stop(); return *this; }
    EffectObject& Speed(float s) { SetSpeed(s); return *this; }
    EffectObject& Paused(bool p) { SetPaused(p); return *this; }
    EffectObject& Shown(bool s) { SetShown(s); return *this; }

    // 便利: その場で生成してシーンに登録して返す（従来）
    static EffectObject* SpawnW(const wchar_t* path,
        const DirectX::XMFLOAT3& pos,
        bool autoDestroyOnEnd = true,
        const DirectX::XMFLOAT3& rotDeg = {0, 0, 0},
        const DirectX::XMFLOAT3& scale = {1, 1, 1});

public: // GameObject
    void Initialize() override;
    void Finalize() override;
    void LogicUpdate(double elapsed_time) override;
    void Draw() override; // 描画は EffectSystem::Draw3D 側で行うため何も描画しない

private:
    int   m_effectId = -1;
    int   m_handle = -1;

    bool  m_autoPlay = true;
    bool  m_autoDestroyOnEnd = true;
};

void Effect_Initialize();
void Effect_Finalize();
void Effect_Update(double elapsed_time);
void Effect_Draw();

#endif // EFFECT_OBJECT_H