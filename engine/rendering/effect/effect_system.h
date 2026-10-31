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
    // Init / shutdown
    bool Initialize();
    void Finalize();

    // Per-frame update (elapsed seconds)
    void Update(double deltaSeconds);

    // Draw 3D effects (uses current camera)
    void Draw3D();

    // Load effect (returns managed ID; -1 on failure)
    int  Load(const char16_t* path);
    int  LoadW(const wchar_t* path);
    int  LoadCached(const wchar_t* path);

    // Play (returns Effekseer handle; -1 on failure)
    int  Play(int effectId, const DirectX::XMFLOAT3& pos);

    // Stop / stop all / reload / exists
    void Stop(int handle);
    void StopAll();
    bool ReloadContent();
    bool Exists(int handle);

    // Transform
    void SetLocation(int handle, const DirectX::XMFLOAT3& pos);
    void SetRotationEulerDeg(int handle, const DirectX::XMFLOAT3& rotDeg);
    void SetScale(int handle, float uniform); // Uniform scale
    void SetScale(int handle, const DirectX::XMFLOAT3& scale); // Per-axis XYZ

    // Playback control
    void SetSpeed(int handle, float speed);
    void SetPaused(int handle, bool paused);
    void SetShown(int handle, bool shown);
}

#endif // !EFFECT_SYSTEM_H
