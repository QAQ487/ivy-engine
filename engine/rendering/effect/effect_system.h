/**===========================================================================
 File   :  [effect_system.h]
 Author : Hayato Otsuki
 Date   : 2025/10/24
 -----------------------------------------------------------------------------
===========================================================================**/
#ifndef EFFECT_SYSTEM_H
#define EFFECT_SYSTEM_H

#include <Effekseer.h>
#include <EffekseerRendererDX11.h>
#include <DirectXMath.h>

namespace EffectSystem {
    // 初期化/終了
    bool Initialize();
    void Finalize();

    // 1フレーム更新（経過秒）
    void Update(double deltaSeconds);

    // 3Dエフェクト描画（現在のカメラを使用）
    void Draw3D();

    // エフェクト読み込み（戻り値は管理ID。-1は失敗）
    int  Load(const char16_t* path);
    int  LoadW(const wchar_t* path);
    int  LoadCached(const wchar_t* path);

    // 再生（戻り値はEffekseerのハンドル。-1は失敗）
    int  Play(int effectId, const DirectX::XMFLOAT3& pos);

    // 停止/全停止/存在確認
    void Stop(int handle);
    void StopAll();
    bool Exists(int handle);

    // Transform 操作
    void SetLocation(int handle, const DirectX::XMFLOAT3& pos);
    void SetRotationEulerDeg(int handle, const DirectX::XMFLOAT3& rotDeg);
    void SetScale(int handle, float uniform); // 一様スケール
    void SetScale(int handle, const DirectX::XMFLOAT3& scale); // XYZ 個別

    // 再生制御
    void SetSpeed(int handle, float speed);
    void SetPaused(int handle, bool paused);
    void SetShown(int handle, bool shown);
}

#endif // !EFFECT_SYSTEM_H