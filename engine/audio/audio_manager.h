/**===========================================================================
 File   :  [audio_manager.h]
 Author : Terry Yeung
 Date   : 2025/11/19
 Brief  : Unified XAudio2 & X3DAudio Manager (Singleton)
          Handles 2D/3D playback, volume control, and resource management.
 ===========================================================================**/
#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <windows.h>
#include <wrl/client.h> // For ComPtr
#include <xaudio2.h>
#include <x3daudio.h>
#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

 // Link necessary libraries
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "winmm.lib")

enum class AudioType {
    BGM,
    SFX,
    VOICE
};

using AudioPlaybackId = std::uint32_t;
static constexpr AudioPlaybackId kInvalidAudioPlaybackId = 0;

struct Playback; // Internal voice + WAV data
class GameObject;

class AudioManager {
public:
    static AudioManager& Instance();

    void Initialize();
    void Finalize();
    void Update();

    void SetListener(const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT3& forward,
        const DirectX::XMFLOAT3& up);

    /**
     * Sets the listener to automatically track a GameObject's position and orientation.
     * The forward and up vectors are calculated automatically from the GameObject's rotation.
     * Call this once during initialization with the player GameObject.
     *
     * IMPORTANT: The GameObject pointer must remain valid while set. Call SetListener(nullptr)
     * or use the manual SetListener(pos, fwd, up) before destroying the GameObject.
     */
    void SetListener(GameObject* gameObject);

    /**
     * Called by GameObject's destructor. Clears the listener pointer and any
     * per-source emitter pointers that reference this object, so a currently
     * playing/tracked sound never dereferences a freed GameObject on the next
     * Update(). You should not need to call this directly.
     */
    void OnGameObjectDestroyed(GameObject* gameObject);

    void PlayBGM(const std::string& id, const std::string& path, float volume = 1.0f, bool loop = true);
    void PlaySFX(const std::string& id, const std::string& path, float volume = 1.0f, bool loop = false);
    void PlayVoice(const std::string& id, const std::string& path, float volume = 1.0f);

    void LoadBGM(const std::string& id, const std::string& path);
    void PlayBGM(const std::string& id, float volume = 1.0f, bool loop = true);
    void LoadSFX(const std::string& id, const std::string& path);
    void PlaySFX(const std::string& id, float volume = 1.0f, bool loop = false);
    void LoadVoice(const std::string& id, const std::string& path);
    void PlayVoice(const std::string& id, float volume = 1.0f);

    void PlaySFX3D(const std::string& id, const std::string& path, const DirectX::XMFLOAT3& position, float volume = 1.0f, bool loop = false);
    void Load3DSFX(const std::string& id, const std::string& path);
    void PlaySFX3D(const std::string& id, float volume = 1.0f, bool loop = false);

    void Stop(const std::string& id);
    void StopAll();
    bool ReloadContent(const std::string& path);
    // True when any loaded/source/instance playback holds this content path.
    bool IsContentLoaded(const std::string& path) const;
    void StopAllByType(AudioType type);
    bool IsPlaying(const std::string& id);

    // Component-owned playback (does not share string ids; overlapping is allowed).
    AudioPlaybackId StartInstance(const std::string& path,
        AudioType type,
        float volume,
        bool loop,
        bool is3D,
        const DirectX::XMFLOAT3& position,
        float distanceScale);
    AudioPlaybackId StartInstanceFromLoaded(const std::string& loadedId, float volume = 1.0f,
        bool persistAcrossScene = false);
    void StopInstance(AudioPlaybackId id);
    bool IsInstancePlaying(AudioPlaybackId id) const;
    void SetInstancePosition(AudioPlaybackId id, const DirectX::XMFLOAT3& position);
    void SetInstanceDistanceScale(AudioPlaybackId id, float scale);
    void SetInstanceVolume(AudioPlaybackId id, float volume);
    void StopAllInstances();
    // Stops component/scene playback; UI click and other persistAcrossScene voices keep going.
    void StopSceneInstances();

    void SetEmitterPosition(const std::string& id, const DirectX::XMFLOAT3& position);
    void SetEmitterDistanceScale(const std::string& id, float scale);
    void SetEmitter(const std::string& id, GameObject* gameObject);
    void SetListenerStereoBalance(bool centerBalance);

    void SetVolume(const std::string& id, float volume);
    void SetMasterVolume(float volume);
    void SetBGMVolume(float volume);
    void SetSFXVolume(float volume);
    void SetVoiceVolume(float volume);

    float GetMasterVolume() const { return m_masterVol; }
    float GetBGMVolume() const { return m_bgmVol; }
    float GetSFXVolume() const { return m_sfxVol; }
    float GetVoiceVolume() const { return m_voiceVol; }

    // Preload a 2D SFX used by UIButton::Activate. Empty path unloads it.
    void SetUiClickSound(const std::string& path);
    void PlayUiClick(float volume = 1.0f);

    bool LoadBusVolumesFromProjectFile(const std::string& projectJsonPath);
    void TryLoadBusVolumesFromNearbyProject();

    void SetDebugDraw(bool enable) { m_debugDraw = enable; }
    void DrawDebug();
    bool GetDebugInfo(
        const std::string& id,
        float* outDistance,
        float* outMatrixVolumeL,
        float* outMatrixVolumeR
    );

private:
    AudioManager() = default;
    ~AudioManager() = default;
    AudioManager(const AudioManager&) = delete;
    void operator=(const AudioManager&) = delete;

    void Apply3DAudio(Playback* source);
    void UpdateVolume(Playback* source);
    bool LoadWav(const std::string& path, Playback* outSource);
    void ReapplyAllVolumes();
    void SweepFinishedInstances();
    void Setup3DEmitter(Playback* source, const DirectX::XMFLOAT3& position, float distanceScale);
    bool SubmitAndStart(Playback* source, bool loop);

    Microsoft::WRL::ComPtr<IXAudio2>               m_xaudio;
    IXAudio2MasteringVoice* m_masterVoice = nullptr;
    X3DAUDIO_HANDLE                                m_x3dInstance = {};

    X3DAUDIO_LISTENER                              m_listener = {};
    GameObject*                                    m_listenerObject = nullptr;
    bool                                           m_listenerStereoBalance = false;

    std::unordered_map<std::string, std::unique_ptr<Playback>> m_sources;
    std::unordered_map<std::string, std::unique_ptr<Playback>> m_loadedAudio;
    std::unordered_map<AudioPlaybackId, std::unique_ptr<Playback>> m_instances;
    AudioPlaybackId m_nextInstanceId = 1;

    float m_masterVol = 1.0f;
    float m_bgmVol = 1.0f;
    float m_sfxVol = 1.0f;
    float m_voiceVol = 1.0f;

    bool  m_debugDraw = false;
    UINT32 m_deviceChannels = 2;
};

#endif // AUDIO_MANAGER_H
