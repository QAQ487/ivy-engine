/**===========================================================================
 File   :  [effect_system.h]
 Author : Terry Yeung
 Date   : 2025/10/24
 -----------------------------------------------------------------------------
===========================================================================**/
#ifndef EFFECT_SYSTEM_H
#define EFFECT_SYSTEM_H

#include <Effekseer.h>
#include <EffekseerRendererDX11.h>
#include <DirectXMath.h>

namespace EffectSystem {
    // ������/�I��
    bool Initialize();
    void Finalize();

    // 1�t���[���X�V�i�o�ߕb�j
    void Update(double deltaSeconds);

    // 3D�G�t�F�N�g�`��i���݂̃J������g�p�j
    void Draw3D();

    // �G�t�F�N�g�ǂݍ��݁i�߂�l�͊Ǘ�ID�B-1�͎��s�j
    int  Load(const char16_t* path);
    int  LoadW(const wchar_t* path);
    int  LoadCached(const wchar_t* path);

    // �Đ��i�߂�l��Effekseer�̃n���h���B-1�͎��s�j
    int  Play(int effectId, const DirectX::XMFLOAT3& pos);

    // ��~/�S��~/���݊m�F
    void Stop(int handle);
    void StopAll();
    bool Exists(int handle);

    // Transform ����
    void SetLocation(int handle, const DirectX::XMFLOAT3& pos);
    void SetRotationEulerDeg(int handle, const DirectX::XMFLOAT3& rotDeg);
    void SetScale(int handle, float uniform); // ��l�X�P�[��
    void SetScale(int handle, const DirectX::XMFLOAT3& scale); // XYZ ��

    // �Đ�����
    void SetSpeed(int handle, float speed);
    void SetPaused(int handle, bool paused);
    void SetShown(int handle, bool shown);
}

#endif // !EFFECT_SYSTEM_H