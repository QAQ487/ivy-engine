/**===========================================================================
 File   :  [audio_manager.h]
 Author : Hayato Otsuki
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

struct AudioSource; // Forward declaration
class GameObject;   // Forward declaration

class AudioManager {
public:
    // --- Singleton Access ---
    static AudioManager& Instance();

    // --- Lifecycle ---
    void Initialize();
    void Finalize();
    void Update(); // Call every frame

    // --- Listener (Player) Control ---
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

    // --- Playback API ---
    void PlayBGM(const std::string& id, const std::string& path, float volume = 1.0f, bool loop = true);
    void PlaySFX(const std::string& id, const std::string& path, float volume = 1.0f, bool loop = false);
    void PlayVoice(const std::string& id, const std::string& path, float volume = 1.0f);

    /**
     * Loads a BGM file to a unique ID without playing it.
     * Call this during initialization to pre-load BGM files.
     */
    void LoadBGM(const std::string& id, const std::string& path);

    /**
     * Plays a previously loaded BGM by ID.
     * The BGM must be loaded first with LoadBGM.
     */
    void PlayBGM(const std::string& id, float volume = 1.0f, bool loop = true);

    /**
     * Loads an SFX file to a unique ID without playing it.
     * Call this during initialization to pre-load SFX files.
     */
    void LoadSFX(const std::string& id, const std::string& path);

    /**
     * Plays a previously loaded SFX by ID.
     * The SFX must be loaded first with LoadSFX.
     */
    void PlaySFX(const std::string& id, float volume = 1.0f, bool loop = false);

    /**
     * Loads a Voice file to a unique ID without playing it.
     * Call this during initialization to pre-load voice files.
     */
    void LoadVoice(const std::string& id, const std::string& path);

    /**
     * Plays a previously loaded Voice by ID.
     * The voice must be loaded first with LoadVoice.
     */
    void PlayVoice(const std::string& id, float volume = 1.0f);

    /**
     * Plays a 3D Sound.
     * Automatically handles Mono/Stereo files correctly for 3D spatialization.
     */
    void PlaySFX3D(const std::string& id, const std::string& path, const DirectX::XMFLOAT3& position, float volume = 1.0f, bool loop = false);

    /**
     * Loads a 3D sound file to a unique ID without playing it.
     * Call this during initialization to pre-load sound files.
     */
    void Load3DSFX(const std::string& id, const std::string& path);

    /**
     * Plays a previously loaded 3D sound by ID.
     * The sound must be loaded first with Load3DSFX.
     */
    void PlaySFX3D(const std::string& id, float volume = 1.0f, bool loop = false);

    // --- Control API ---
    void Stop(const std::string& id);
    void StopAll();
    void StopAllByType(AudioType type);
    bool IsPlaying(const std::string& id);

    // --- Emitter Control ---
    void SetEmitterPosition(const std::string& id, const DirectX::XMFLOAT3& position);
    void SetEmitterDistanceScale(const std::string& id, float scale);
    
    /**
     * Sets an emitter to automatically track a GameObject's position and orientation.
     * The forward and up vectors are calculated automatically from the GameObject's rotation.
     * Call this once during initialization or when starting to play a 3D sound.
     * 
     * IMPORTANT: The GameObject pointer must remain valid while the sound is playing.
     * Call Stop(id) or SetEmitter(id, nullptr) before destroying the GameObject.
     */
    void SetEmitter(const std::string& id, GameObject* gameObject);
    
    /**
     * Sets stereo balance mode for the listener (affects all 3D sounds received).
     * When enabled (true), all 3D sounds will have equal volume in both speakers regardless of position.
     * When disabled (false, default), 3D sounds use spatial panning based on direction.
     * Useful for accessibility or when directional audio is not desired.
     */
    void SetListenerStereoBalance(bool centerBalance);

    // --- Volume Control ---
    void SetVolume(const std::string& id, float volume);
    void SetMasterVolume(float volume);
    void SetBGMVolume(float volume);
    void SetSFXVolume(float volume);
    void SetVoiceVolume(float volume);

    // --- Debug ---
    void SetDebugDraw(bool enable) { m_debugDraw = enable; }
    void DrawDebug(); // Call in Draw loop
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

    // Internal Helpers
    void Apply3DAudio(AudioSource* source);
    void UpdateVolume(AudioSource* source);
    bool LoadWav(const std::string& path, AudioSource* outSource);

    // XAudio2 Core
    Microsoft::WRL::ComPtr<IXAudio2>               m_xaudio;
    IXAudio2MasteringVoice* m_masterVoice = nullptr;
    X3DAUDIO_HANDLE                                m_x3dInstance = {};

    // Listener Data
    X3DAUDIO_LISTENER                              m_listener = {};
    GameObject*                                    m_listenerObject = nullptr; // GameObject to track for listener
    bool                                           m_listenerStereoBalance = false; // True to disable L/R panning globally

    // Resources
    std::unordered_map<std::string, std::unique_ptr<AudioSource>> m_sources;
    std::unordered_map<std::string, std::unique_ptr<AudioSource>> m_loadedAudio; // Pre-loaded audio (BGM, SFX, Voice, 3D SFX)

    // Settings
    float m_masterVol = 1.0f;
    float m_bgmVol = 1.0f;
    float m_sfxVol = 1.0f;
    float m_voiceVol = 1.0f;

    bool  m_debugDraw = false;
    UINT32 m_deviceChannels = 2;
};

#endif // AUDIO_MANAGER_H