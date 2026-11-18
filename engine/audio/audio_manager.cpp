/**===========================================================================
 File   :  [audio_manager.cpp]
 Author : Terry Yeung
 Date   : 2025/11/19
 ===========================================================================**/
#include "audio_manager.h"
#include "engine/ecs/game_object.h"
#include "engine/debug_draw/debug_drawer.h"
#include "engine/core/utf8_path.h"
#include "engine/core/content_path.h"
#include "engine/audio/wav_parse.h"
#include "libraries/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iterator>

 // Internal structure to hold all data for a single sound
struct Playback {
    std::string contentPath;
    AudioType type;
    bool      is3D;
    bool      isLooping;
    float     baseVolume; // The volume set by Play()
    bool      isLoaded = false; // True if loaded but not yet playing
    bool      persistAcrossScene = false;
    GameObject* gameObject = nullptr; // GameObject to track for position/orientation

    // XAudio2 Data
    IXAudio2SourceVoice* sourceVoice = nullptr;
    std::vector<BYTE>                           audioData; // Owns the raw WAV data
    WAVEFORMATEX                                wfx;

    // 3D Data
    DirectX::XMFLOAT3           position;
    X3DAUDIO_EMITTER            emitter;
    X3DAUDIO_DSP_SETTINGS       dspSettings;
    std::vector<float>          matrixCoefficients; // Dynamic storage for matrix
};

// Distance curve: 1.0 at dist 0 -> 0.0 at dist 1
static X3DAUDIO_DISTANCE_CURVE_POINT g_linearCurvePoints[2] = {
    { 0.0f, 1.0f },
    { 1.0f, 0.0f }
};
static X3DAUDIO_DISTANCE_CURVE g_linearCurve = { g_linearCurvePoints, 2 };

AudioManager& AudioManager::Instance() {
    static AudioManager instance;
    return instance;
}

void AudioManager::Initialize() {
    // 1. Init XAudio2
    HRESULT hr = XAudio2Create(m_xaudio.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) return;

    // 2. Create Mastering Voice
    hr = m_xaudio->CreateMasteringVoice(&m_masterVoice);
    if (FAILED(hr)) return;

    // 3. Init X3DAudio
    DWORD channelMask;
    m_masterVoice->GetChannelMask(&channelMask);
    X3DAudioInitialize(channelMask, X3DAUDIO_SPEED_OF_SOUND, m_x3dInstance);

    // 4. Store destination channels (usually 2 for stereo)
    XAUDIO2_VOICE_DETAILS details;
    m_masterVoice->GetVoiceDetails(&details);
    m_deviceChannels = details.InputChannels;

    // 5. Default Listener
    m_listener.Position = { 0, 0, 0 };
    m_listener.OrientFront = { 0, 0, 1 };
    m_listener.OrientTop = { 0, 1, 0 };
    m_listener.Velocity = { 0, 0, 0 };
}

namespace {
constexpr const char* kUiClickLoadedId = "__engine_ui_click";

float Clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

void DestroyPlaybackVoice(Playback* playback) {
    if (!playback || !playback->sourceVoice) return;
    playback->sourceVoice->Stop();
    playback->sourceVoice->DestroyVoice();
    playback->sourceVoice = nullptr;
}

bool VoiceHasFinished(Playback* playback) {
    if (!playback || !playback->sourceVoice) return true;
    if (playback->isLooping) return false;
    XAUDIO2_VOICE_STATE state = {};
    playback->sourceVoice->GetState(&state);
    return state.BuffersQueued == 0;
}
}

void AudioManager::Finalize() {
    StopAll();
    if (m_masterVoice) {
        m_masterVoice->DestroyVoice();
        m_masterVoice = nullptr;
    }
    m_xaudio.Reset();
    CoUninitialize();
}

void AudioManager::Update() {
    if (m_listenerObject) {
        DirectX::XMFLOAT3 pos = m_listenerObject->GetPosition();
        DirectX::XMFLOAT3 rot = m_listenerObject->GetRotation();

        float yawRad = DirectX::XMConvertToRadians(rot.y);
        DirectX::XMFLOAT3 forward = {
            std::sinf(yawRad),
            0.0f,
            std::cosf(yawRad)
        };
        DirectX::XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };

        m_listener.Position = pos;
        m_listener.OrientFront = forward;
        m_listener.OrientTop = up;
    }

    auto applyEmitters = [this](auto& map) {
        for (auto& pair : map) {
            if (!pair.second || !pair.second->is3D) continue;
            if (pair.second->gameObject) {
                pair.second->position = pair.second->gameObject->GetPosition();
                pair.second->emitter.Position = pair.second->position;
            }
            Apply3DAudio(pair.second.get());
        }
    };
    applyEmitters(m_sources);
    applyEmitters(m_instances);
    SweepFinishedInstances();
}

// ---------------------------------------------------------------------------
// Helper: Load WAV from disk
// ---------------------------------------------------------------------------
bool AudioManager::LoadWav(const std::string& path, Playback* outSource) {
    outSource->contentPath = path;

    // UTF-8 safe: rom/ paths resolve project-then-resources; other paths stay
    // literal. mmioOpenA decoded the path as the ANSI code page instead.
    std::vector<std::uint8_t> bytes;
    try {
        const std::filesystem::path file = content_path::ReadableFileCurrent(path);
        if (file.empty()) return false;
        std::ifstream in(file, std::ios::binary);
        if (!in) return false;
        bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    catch (...) {
        return false;
    }

    wav_parse::WavChunks chunks;
    if (!wav_parse::ParseRiffWave(bytes.data(), bytes.size(), &chunks)) return false;

    memset(&outSource->wfx, 0, sizeof(WAVEFORMATEX));
    if (chunks.fmtSize >= sizeof(WAVEFORMATEX)) {
        memcpy(&outSource->wfx, bytes.data() + chunks.fmtOffset, sizeof(WAVEFORMATEX));
    }
    else {
        // PCMWAVEFORMAT (16 bytes): no cbSize field.
        memcpy(&outSource->wfx, bytes.data() + chunks.fmtOffset, sizeof(PCMWAVEFORMAT));
        outSource->wfx.cbSize = 0;
    }

    const auto dataBegin = bytes.begin() + static_cast<std::ptrdiff_t>(chunks.dataOffset);
    outSource->audioData.assign(dataBegin, dataBegin + static_cast<std::ptrdiff_t>(chunks.dataSize));
    return true;
}

// ---------------------------------------------------------------------------
// Play Logic
// ---------------------------------------------------------------------------
void AudioManager::PlaySFX3D(const std::string& id, const std::string& path, const DirectX::XMFLOAT3& position, float volume, bool loop) {
    Stop(id); // Replace if exists

    auto source = std::make_unique<Playback>();
    if (!LoadWav(path, source.get())) return;

    source->type = AudioType::SFX;
    source->is3D = true;
    source->isLooping = loop;
    source->baseVolume = volume;
    source->position = position;

    // Create Source Voice
    if (FAILED(m_xaudio->CreateSourceVoice(&source->sourceVoice, &source->wfx))) return;

    // --- 3D Setup (The Fix) ---
    // We force the Emitter calculation to be MONO (1 channel) regardless of the file.
    source->emitter = { 0 };
    source->emitter.ChannelCount = 1;
    source->emitter.CurveDistanceScaler = 20.0f; // Default range
    source->emitter.DopplerScaler = 1.0f;
    source->emitter.pVolumeCurve = &g_linearCurve;
    source->emitter.Position = position;

    // DSP Settings
    source->dspSettings = { 0 };
    source->dspSettings.SrcChannelCount = 1; // Match emitter
    source->dspSettings.DstChannelCount = m_deviceChannels;

    // Allocate matrix memory
    source->matrixCoefficients.resize(source->dspSettings.SrcChannelCount * source->dspSettings.DstChannelCount);
    source->dspSettings.pMatrixCoefficients = source->matrixCoefficients.data();

    // Submit Buffer
    XAUDIO2_BUFFER buffer = { 0 };
    buffer.AudioBytes = (UINT32)source->audioData.size();
    buffer.pAudioData = source->audioData.data();
    buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

    source->sourceVoice->SubmitSourceBuffer(&buffer);
    source->sourceVoice->Start();

    // Initial 3D Calc
    Apply3DAudio(source.get());

    m_sources[id] = std::move(source);
}

void AudioManager::PlaySFX(const std::string& id, const std::string& path, float volume, bool loop) {
    Stop(id);

    auto source = std::make_unique<Playback>();
    if (!LoadWav(path, source.get())) return;

    source->type = AudioType::SFX;
    source->is3D = false;
    source->isLooping = loop;
    source->baseVolume = volume;

    if (FAILED(m_xaudio->CreateSourceVoice(&source->sourceVoice, &source->wfx))) return;

    XAUDIO2_BUFFER buffer = { 0 };
    buffer.AudioBytes = (UINT32)source->audioData.size();
    buffer.pAudioData = source->audioData.data();
    buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

    source->sourceVoice->SubmitSourceBuffer(&buffer);
    source->sourceVoice->Start();

    UpdateVolume(source.get());
    m_sources[id] = std::move(source);
}

void AudioManager::PlayBGM(const std::string& id, const std::string& path, float volume, bool loop) {
    Stop(id);
    auto source = std::make_unique<Playback>();
    if (!LoadWav(path, source.get())) return;

    source->type = AudioType::BGM;
    source->is3D = false;
    source->isLooping = loop;
    source->baseVolume = volume;

    if (FAILED(m_xaudio->CreateSourceVoice(&source->sourceVoice, &source->wfx))) return;

    XAUDIO2_BUFFER buffer = { 0 };
    buffer.AudioBytes = (UINT32)source->audioData.size();
    buffer.pAudioData = source->audioData.data();
    buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

    source->sourceVoice->SubmitSourceBuffer(&buffer);
    source->sourceVoice->Start();
    UpdateVolume(source.get());

    m_sources[id] = std::move(source);
}

void AudioManager::PlayVoice(const std::string& id, const std::string& path, float volume) {
    Stop(id);
    auto source = std::make_unique<Playback>();
    if (!LoadWav(path, source.get())) return;

    source->type = AudioType::VOICE;
    source->is3D = false;
    source->isLooping = false;
    source->baseVolume = volume;

    if (FAILED(m_xaudio->CreateSourceVoice(&source->sourceVoice, &source->wfx))) return;

    XAUDIO2_BUFFER buffer = { 0 };
    buffer.AudioBytes = (UINT32)source->audioData.size();
    buffer.pAudioData = source->audioData.data();

    source->sourceVoice->SubmitSourceBuffer(&buffer);
    source->sourceVoice->Start();
    UpdateVolume(source.get());

    m_sources[id] = std::move(source);
}

// ---------------------------------------------------------------------------
// Load and Play API for BGM, SFX, Voice, and 3D SFX
// ---------------------------------------------------------------------------
void AudioManager::LoadBGM(const std::string& id, const std::string& path) {
    // Remove any existing loaded source with this ID
    m_loadedAudio.erase(id);
    
    auto source = std::make_unique<Playback>();
    if (!LoadWav(path, source.get())) return;
    
    source->type = AudioType::BGM;
    source->is3D = false;
    source->isLooping = true; // BGM typically loops
    source->baseVolume = 1.0f;
    source->isLoaded = true;
    
    m_loadedAudio[id] = std::move(source);
}

void AudioManager::PlayBGM(const std::string& id, float volume, bool loop) {
    // Find the loaded source
    auto it = m_loadedAudio.find(id);
    if (it == m_loadedAudio.end()) {
        // Sound not loaded, cannot play
        return;
    }
    
    Stop(id); // Stop any currently playing instance
    
    // Create a new source and perform deep copy of loaded data
    auto source = std::make_unique<Playback>();
    const Playback* loaded = it->second.get();
    
    // Copy basic properties
    source->type = loaded->type;
    source->is3D = loaded->is3D;
    source->isLooping = loop;
    source->baseVolume = volume;
    source->isLoaded = false; // Now it's playing, not just loaded
    
    // Deep copy audio data
    source->audioData = loaded->audioData; // std::vector does deep copy
    source->wfx = loaded->wfx;
    
    // Create Source Voice
    if (FAILED(m_xaudio->CreateSourceVoice(&source->sourceVoice, &source->wfx))) return;
    
    // Submit Buffer
    XAUDIO2_BUFFER buffer = { 0 };
    buffer.AudioBytes = (UINT32)source->audioData.size();
    buffer.pAudioData = source->audioData.data();
    buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;
    
    source->sourceVoice->SubmitSourceBuffer(&buffer);
    source->sourceVoice->Start();
    UpdateVolume(source.get());
    
    m_sources[id] = std::move(source);
}

void AudioManager::LoadSFX(const std::string& id, const std::string& path) {
    // Remove any existing loaded source with this ID
    m_loadedAudio.erase(id);
    
    auto source = std::make_unique<Playback>();
    if (!LoadWav(path, source.get())) return;
    
    source->type = AudioType::SFX;
    source->is3D = false;
    source->isLooping = false;
    source->baseVolume = 1.0f;
    source->isLoaded = true;
    
    m_loadedAudio[id] = std::move(source);
}

void AudioManager::PlaySFX(const std::string& id, float volume, bool loop) {
    // Find the loaded source
    auto it = m_loadedAudio.find(id);
    if (it == m_loadedAudio.end()) {
        // Sound not loaded, cannot play
        return;
    }
    
    Stop(id); // Stop any currently playing instance
    
    // Create a new source and perform deep copy of loaded data
    auto source = std::make_unique<Playback>();
    const Playback* loaded = it->second.get();
    
    // Copy basic properties
    source->type = loaded->type;
    source->is3D = loaded->is3D;
    source->isLooping = loop;
    source->baseVolume = volume;
    source->isLoaded = false; // Now it's playing, not just loaded
    
    // Deep copy audio data
    source->audioData = loaded->audioData; // std::vector does deep copy
    source->wfx = loaded->wfx;
    
    // Create Source Voice
    if (FAILED(m_xaudio->CreateSourceVoice(&source->sourceVoice, &source->wfx))) return;
    
    // Submit Buffer
    XAUDIO2_BUFFER buffer = { 0 };
    buffer.AudioBytes = (UINT32)source->audioData.size();
    buffer.pAudioData = source->audioData.data();
    buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;
    
    source->sourceVoice->SubmitSourceBuffer(&buffer);
    source->sourceVoice->Start();
    UpdateVolume(source.get());
    
    m_sources[id] = std::move(source);
}

void AudioManager::LoadVoice(const std::string& id, const std::string& path) {
    // Remove any existing loaded source with this ID
    m_loadedAudio.erase(id);
    
    auto source = std::make_unique<Playback>();
    if (!LoadWav(path, source.get())) return;
    
    source->type = AudioType::VOICE;
    source->is3D = false;
    source->isLooping = false;
    source->baseVolume = 1.0f;
    source->isLoaded = true;
    
    m_loadedAudio[id] = std::move(source);
}

void AudioManager::PlayVoice(const std::string& id, float volume) {
    // Find the loaded source
    auto it = m_loadedAudio.find(id);
    if (it == m_loadedAudio.end()) {
        // Sound not loaded, cannot play
        return;
    }
    
    Stop(id); // Stop any currently playing instance
    
    // Create a new source and perform deep copy of loaded data
    auto source = std::make_unique<Playback>();
    const Playback* loaded = it->second.get();
    
    // Copy basic properties
    source->type = loaded->type;
    source->is3D = loaded->is3D;
    source->isLooping = false; // Voice never loops
    source->baseVolume = volume;
    source->isLoaded = false; // Now it's playing, not just loaded
    
    // Deep copy audio data
    source->audioData = loaded->audioData; // std::vector does deep copy
    source->wfx = loaded->wfx;
    
    // Create Source Voice
    if (FAILED(m_xaudio->CreateSourceVoice(&source->sourceVoice, &source->wfx))) return;
    
    // Submit Buffer
    XAUDIO2_BUFFER buffer = { 0 };
    buffer.AudioBytes = (UINT32)source->audioData.size();
    buffer.pAudioData = source->audioData.data();
    buffer.LoopCount = 0; // Voice never loops
    
    source->sourceVoice->SubmitSourceBuffer(&buffer);
    source->sourceVoice->Start();
    UpdateVolume(source.get());
    
    m_sources[id] = std::move(source);
}

void AudioManager::Load3DSFX(const std::string& id, const std::string& path) {
    // Remove any existing loaded source with this ID
    m_loadedAudio.erase(id);
    
    auto source = std::make_unique<Playback>();
    if (!LoadWav(path, source.get())) return;
    
    source->type = AudioType::SFX;
    source->is3D = true;
    source->isLooping = false;
    source->baseVolume = 1.0f;
    source->isLoaded = true;
    
    // Initialize 3D emitter structure
    source->emitter = { 0 };
    source->emitter.ChannelCount = 1;
    source->emitter.CurveDistanceScaler = 20.0f; // Default range
    source->emitter.DopplerScaler = 1.0f;
    source->emitter.pVolumeCurve = &g_linearCurve;
    source->emitter.Position = { 0.0f, 0.0f, 0.0f }; // Will be set when played
    
    // DSP Settings
    source->dspSettings = { 0 };
    source->dspSettings.SrcChannelCount = 1;
    source->dspSettings.DstChannelCount = m_deviceChannels;
    
    // Allocate matrix memory
    source->matrixCoefficients.resize(source->dspSettings.SrcChannelCount * source->dspSettings.DstChannelCount);
    source->dspSettings.pMatrixCoefficients = source->matrixCoefficients.data();
    
    m_loadedAudio[id] = std::move(source);
}

void AudioManager::PlaySFX3D(const std::string& id, float volume, bool loop) {
    // Find the loaded source
    auto it = m_loadedAudio.find(id);
    if (it == m_loadedAudio.end()) {
        // Sound not loaded, cannot play
        return;
    }
    
    Stop(id); // Stop any currently playing instance
    
    // Create a new source and perform deep copy of loaded data
    auto source = std::make_unique<Playback>();
    const Playback* loaded = it->second.get();
    
    // Copy basic properties
    source->type = loaded->type;
    source->is3D = loaded->is3D;
    source->isLooping = loop;
    source->baseVolume = volume;
    source->isLoaded = false; // Now it's playing, not just loaded
    source->gameObject = nullptr; // Will be set separately if needed
    
    // Deep copy audio data
    source->audioData = loaded->audioData; // std::vector does deep copy
    source->wfx = loaded->wfx;
    
    // Copy 3D data
    source->position = loaded->position;
    source->emitter = loaded->emitter;
    
    // Initialize DSP settings with same structure
    source->dspSettings = { 0 };
    source->dspSettings.SrcChannelCount = loaded->dspSettings.SrcChannelCount;
    source->dspSettings.DstChannelCount = loaded->dspSettings.DstChannelCount;
    
    // Allocate new matrix coefficients memory and initialize to zero
    source->matrixCoefficients.resize(source->dspSettings.SrcChannelCount * source->dspSettings.DstChannelCount, 0.0f);
    source->dspSettings.pMatrixCoefficients = source->matrixCoefficients.data();
    
    // Create Source Voice
    if (FAILED(m_xaudio->CreateSourceVoice(&source->sourceVoice, &source->wfx))) return;
    
    // Submit Buffer
    XAUDIO2_BUFFER buffer = { 0 };
    buffer.AudioBytes = (UINT32)source->audioData.size();
    buffer.pAudioData = source->audioData.data();
    buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;
    
    source->sourceVoice->SubmitSourceBuffer(&buffer);
    source->sourceVoice->Start();
    
    // Initial 3D Calc (this will properly initialize the matrix coefficients)
    Apply3DAudio(source.get());
    
    m_sources[id] = std::move(source);
}

// ---------------------------------------------------------------------------
// 3D Calculation (The optimized logic)
// ---------------------------------------------------------------------------
void AudioManager::Apply3DAudio(Playback* source) {
    if (!source || !source->is3D || !source->sourceVoice) return;

    // 1. Update Emitter Position
    source->emitter.Position = source->position;

    // 2. Calculate (Mono -> Stereo)
    X3DAudioCalculate(m_x3dInstance, &m_listener, &source->emitter,
        X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER,
        &source->dspSettings);

    // 3. Calculate Volume Multiplier
    float typeVol = (source->type == AudioType::BGM) ? m_bgmVol :
        (source->type == AudioType::SFX) ? m_sfxVol : m_voiceVol;
    float finalVol = m_masterVol * typeVol * source->baseVolume;

    // 4. Apply Volume to Matrix
    // We do this manually because SetVolume() is global, but Matrix handles panning
    for (size_t i = 0; i < source->matrixCoefficients.size(); ++i) {
        source->matrixCoefficients[i] *= finalVol;
    }

    // 4.5. Apply Listener Center Balance if enabled (equal volume in both speakers)
    if (m_listenerStereoBalance && source->matrixCoefficients.size() >= 2) {
        // Average the left and right volumes for equal balance
        float avgVol = (source->matrixCoefficients[0] + source->matrixCoefficients[1]) / 2.0f;
        source->matrixCoefficients[0] = avgVol;
        source->matrixCoefficients[1] = avgVol;
    }

    // 5. Apply to Voice (Handling Stereo Files correctly)
    int voiceChannels = source->wfx.nChannels;

    if (voiceChannels == 1) {
        // Mono File: Direct mapping
        source->sourceVoice->SetOutputMatrix(nullptr, 1, m_deviceChannels, source->matrixCoefficients.data());
    }
    else if (voiceChannels == 2) {
        // Stereo File: Expand Mono calculation to both channels
        // The calculated matrix is [L_Vol, R_Vol] (size 2)
        // We need [L_Vol, R_Vol, L_Vol, R_Vol] (size 4)
        std::vector<float> expanded(2 * m_deviceChannels);
        float* calculated = source->matrixCoefficients.data();

        // Copy the calculated 3D volumes to both Left and Right source channels
        for (UINT32 d = 0; d < m_deviceChannels; ++d) {
            expanded[d] = calculated[d];                    // Source Left  -> Speaker "d"
            expanded[d + m_deviceChannels] = calculated[d]; // Source Right -> Speaker "d"
        }

        source->sourceVoice->SetOutputMatrix(
            nullptr,
            2,
            m_deviceChannels,
            expanded.data()
        );
    }

    // Doppler
    source->sourceVoice->SetFrequencyRatio(source->dspSettings.DopplerFactor);
}

// ---------------------------------------------------------------------------
// General Controls
// ---------------------------------------------------------------------------
void AudioManager::Stop(const std::string& id) {
    auto it = m_sources.find(id);
    if (it != m_sources.end()) {
        if (it->second->sourceVoice) {
            it->second->sourceVoice->Stop();
            it->second->sourceVoice->DestroyVoice();
            it->second->sourceVoice = nullptr;
        }
        m_sources.erase(it); // Unique_ptr handles cleanup of audioData
    }
}

void AudioManager::StopAll() {
    for (auto& pair : m_sources)
        DestroyPlaybackVoice(pair.second.get());
    m_sources.clear();
    StopAllInstances();
}

void AudioManager::StopAllByType(AudioType type) {
    auto it = m_sources.begin();

    while (it != m_sources.end()) {
        // Check if this sound matches the target type
        if (it->second->type == type) {

            // 1. Stop and Destroy the XAudio2 Voice
            if (it->second->sourceVoice) {
                it->second->sourceVoice->Stop();
                it->second->sourceVoice->DestroyVoice();
                it->second->sourceVoice = nullptr;
            }

            // 2. Erase from map. 
            // m_sources.erase(it) returns the iterator to the NEXT element.
            it = m_sources.erase(it);

        }
        else {
            // Move to next element manually
            ++it;
        }
    }
}

bool AudioManager::IsPlaying(const std::string& id) {
    // Check if the sound exists in our active sources map
    return m_sources.find(id) != m_sources.end();
}

void AudioManager::SetEmitterPosition(const std::string& id, const DirectX::XMFLOAT3& position) {
    auto it = m_sources.find(id);

    // Only update if the sound exists AND is a 3D sound
    if (it != m_sources.end() && it->second->is3D) {
        // Update the storage position
        it->second->position = position;

        // Update the actual X3DAudio emitter struct
        // The next call to Update() -> Apply3DAudio() will use this new position
        it->second->emitter.Position = position;
    }
}

void AudioManager::SetEmitterDistanceScale(const std::string& id, float scale) {
    auto it = m_sources.find(id);

    if (it != m_sources.end() && it->second->is3D) {
        // Safety check: Distance scale cannot be negative
        if (scale < 0.0f) scale = 0.0f;

        // Update the curve scaler
        // 20.0f means the sound fades to silence at 20 units away
        it->second->emitter.CurveDistanceScaler = scale;
    }
}

void AudioManager::SetEmitter(const std::string& id, GameObject* gameObject) {
    auto it = m_sources.find(id);
    
    if (it != m_sources.end() && it->second->is3D) {
        it->second->gameObject = gameObject;
    }
}

void AudioManager::SetListenerStereoBalance(bool centerBalance) {
    m_listenerStereoBalance = centerBalance;
    
    // Re-apply 3D audio to all active 3D sounds to update balance immediately
    for (auto& pair : m_sources) {
        if (pair.second->is3D) {
            Apply3DAudio(pair.second.get());
        }
    }
    for (auto& pair : m_instances) {
        if (pair.second && pair.second->is3D) {
            Apply3DAudio(pair.second.get());
        }
    }
}

void AudioManager::SetListener(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& fwd, const DirectX::XMFLOAT3& up) {
    m_listener.Position = pos;
    m_listener.OrientFront = fwd;
    m_listener.OrientTop = up;
}

void AudioManager::SetListener(GameObject* gameObject) {
    m_listenerObject = gameObject;
}

void AudioManager::OnGameObjectDestroyed(GameObject* gameObject) {
    if (!gameObject) return;

    if (m_listenerObject == gameObject)
        m_listenerObject = nullptr;

    for (auto& pair : m_sources) {
        if (pair.second && pair.second->gameObject == gameObject)
            pair.second->gameObject = nullptr;
    }
    for (auto& pair : m_loadedAudio) {
        if (pair.second && pair.second->gameObject == gameObject)
            pair.second->gameObject = nullptr;
    }
    for (auto& pair : m_instances) {
        if (pair.second && pair.second->gameObject == gameObject)
            pair.second->gameObject = nullptr;
    }
}

void AudioManager::SetVolume(const std::string& id, float volume) {
    if (m_sources.count(id)) {
        m_sources[id]->baseVolume = volume;
        if (m_sources[id]->is3D) Apply3DAudio(m_sources[id].get());
        else UpdateVolume(m_sources[id].get());
    }
}

void AudioManager::UpdateVolume(Playback* source) {
    if (!source || !source->sourceVoice) return;
    float typeVol = (source->type == AudioType::BGM) ? m_bgmVol :
        (source->type == AudioType::SFX) ? m_sfxVol : m_voiceVol;
    source->sourceVoice->SetVolume(m_masterVol * typeVol * source->baseVolume);
}

void AudioManager::ReapplyAllVolumes() {
    auto apply = [this](auto& map) {
        for (auto& pair : map) {
            if (!pair.second) continue;
            if (pair.second->is3D) Apply3DAudio(pair.second.get());
            else UpdateVolume(pair.second.get());
        }
    };
    apply(m_sources);
    apply(m_instances);
}

void AudioManager::SetMasterVolume(float v) { m_masterVol = Clamp01(v); ReapplyAllVolumes(); }
void AudioManager::SetBGMVolume(float v) { m_bgmVol = Clamp01(v); ReapplyAllVolumes(); }
void AudioManager::SetSFXVolume(float v) { m_sfxVol = Clamp01(v); ReapplyAllVolumes(); }
void AudioManager::SetVoiceVolume(float v) { m_voiceVol = Clamp01(v); ReapplyAllVolumes(); }

// Debug
void AudioManager::DrawDebug() {
    if (!m_debugDraw) return;

    static DebugDrawer drawer; // Assuming singleton or similar availability
    static bool init = false;
    if (!init) { drawer.Initialize(); init = true; }

    for (const auto& pair : m_sources) {
        if (pair.second && pair.second->is3D) {
            drawer.DrawSphere(pair.second->position, { 0,0,0 }, pair.second->emitter.CurveDistanceScaler, { 0,0,0 });
        }
    }
    for (const auto& pair : m_instances) {
        if (pair.second && pair.second->is3D) {
            drawer.DrawSphere(pair.second->position, { 0,0,0 }, pair.second->emitter.CurveDistanceScaler, { 0,0,0 });
        }
    }
}

bool AudioManager::GetDebugInfo(
    const std::string& id,
    float* outDistance,
    float* outMatrixVolumeL,
    float* outMatrixVolumeR
)
{
    // 1. Use m_sources (class member)
    auto it = m_sources.find(id);
    if (it == m_sources.end() || !it->second->is3D) {
        return false;
    }

    const Playback* source = it->second.get();

    // 2. Calculate distance from listener to emitter
    // Use m_listener (class member)
    DirectX::XMVECTOR listenerPos = DirectX::XMLoadFloat3(&m_listener.Position);
    DirectX::XMVECTOR emitterPos = DirectX::XMLoadFloat3(&source->emitter.Position);
    DirectX::XMVECTOR distanceVec = DirectX::XMVectorSubtract(emitterPos, listenerPos);
    DirectX::XMVECTOR distanceLength = DirectX::XMVector3Length(distanceVec);

    if (outDistance) {
        DirectX::XMStoreFloat(outDistance, distanceLength);
    }

    // 3. Extract matrix coefficients (Left/Right Volume)
    UINT32 deviceChannels = m_deviceChannels;

    // Check if the device has at least two channels (Stereo) and if the matrix has enough data
    if (deviceChannels >= 2 && source->matrixCoefficients.size() >= 2) {
        // Channel 0 is typically Front Left
        if (outMatrixVolumeL) {
            *outMatrixVolumeL = source->matrixCoefficients[0];
        }
        // Channel 1 is typically Front Right
        if (outMatrixVolumeR) {
            *outMatrixVolumeR = source->matrixCoefficients[1];
        }
    }
    else if (deviceChannels == 1 && source->matrixCoefficients.size() >= 1) {
        // Handle Mono Case: Output volume to Left, Right is 0
        if (outMatrixVolumeL) {
            *outMatrixVolumeL = source->matrixCoefficients[0];
        }
        if (outMatrixVolumeR) {
            *outMatrixVolumeR = 0.0f;
        }
    }
    else {
        // Default to 0 if channels or matrix are unavailable/mismatched
        if (outMatrixVolumeL) *outMatrixVolumeL = 0.0f;
        if (outMatrixVolumeR) *outMatrixVolumeR = 0.0f;
    }

    return true;
}

void AudioManager::Setup3DEmitter(Playback* source, const DirectX::XMFLOAT3& position, float distanceScale) {
    if (!source) return;
    source->emitter = { 0 };
    source->emitter.ChannelCount = 1;
    source->emitter.CurveDistanceScaler = (distanceScale > 0.0f) ? distanceScale : 20.0f;
    source->emitter.DopplerScaler = 1.0f;
    source->emitter.pVolumeCurve = &g_linearCurve;
    source->emitter.Position = position;
    source->dspSettings = { 0 };
    source->dspSettings.SrcChannelCount = 1;
    source->dspSettings.DstChannelCount = m_deviceChannels;
    source->matrixCoefficients.resize(
        source->dspSettings.SrcChannelCount * source->dspSettings.DstChannelCount, 0.0f);
    source->dspSettings.pMatrixCoefficients = source->matrixCoefficients.data();
}

bool AudioManager::SubmitAndStart(Playback* source, bool loop) {
    if (!source || !m_xaudio) return false;
    if (FAILED(m_xaudio->CreateSourceVoice(&source->sourceVoice, &source->wfx)))
        return false;

    XAUDIO2_BUFFER buffer = { 0 };
    buffer.AudioBytes = (UINT32)source->audioData.size();
    buffer.pAudioData = source->audioData.data();
    buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;
    source->sourceVoice->SubmitSourceBuffer(&buffer);
    source->sourceVoice->Start();
    return true;
}

void AudioManager::SweepFinishedInstances() {
    auto it = m_instances.begin();
    while (it != m_instances.end()) {
        if (VoiceHasFinished(it->second.get())) {
            DestroyPlaybackVoice(it->second.get());
            it = m_instances.erase(it);
        }
        else {
            ++it;
        }
    }
}

AudioPlaybackId AudioManager::StartInstance(const std::string& path,
    AudioType type,
    float volume,
    bool loop,
    bool is3D,
    const DirectX::XMFLOAT3& position,
    float distanceScale)
{
    if (!m_xaudio || path.empty())
        return kInvalidAudioPlaybackId;

    auto playback = std::make_unique<Playback>();
    if (!LoadWav(path, playback.get()))
        return kInvalidAudioPlaybackId;

    playback->type = type;
    playback->is3D = is3D;
    playback->isLooping = loop;
    playback->baseVolume = Clamp01(volume);
    playback->isLoaded = false;
    playback->position = position;

    if (is3D)
        Setup3DEmitter(playback.get(), position, distanceScale);

    if (!SubmitAndStart(playback.get(), loop))
        return kInvalidAudioPlaybackId;

    if (is3D)
        Apply3DAudio(playback.get());
    else
        UpdateVolume(playback.get());

    const AudioPlaybackId id = m_nextInstanceId++;
    if (m_nextInstanceId == kInvalidAudioPlaybackId)
        m_nextInstanceId = 1;
    m_instances[id] = std::move(playback);
    return id;
}

AudioPlaybackId AudioManager::StartInstanceFromLoaded(const std::string& loadedId, float volume,
    bool persistAcrossScene)
{
    if (!m_xaudio || loadedId.empty())
        return kInvalidAudioPlaybackId;

    auto it = m_loadedAudio.find(loadedId);
    if (it == m_loadedAudio.end() || !it->second)
        return kInvalidAudioPlaybackId;

    const Playback* loaded = it->second.get();
    auto playback = std::make_unique<Playback>();
    playback->contentPath = loaded->contentPath;
    playback->type = loaded->type;
    playback->is3D = loaded->is3D;
    playback->isLooping = false;
    playback->baseVolume = Clamp01(volume);
    playback->isLoaded = false;
    playback->persistAcrossScene = persistAcrossScene;
    playback->audioData = loaded->audioData;
    playback->wfx = loaded->wfx;
    playback->position = loaded->position;

    if (playback->is3D)
        Setup3DEmitter(playback.get(), playback->position, loaded->emitter.CurveDistanceScaler);

    if (!SubmitAndStart(playback.get(), false))
        return kInvalidAudioPlaybackId;

    if (playback->is3D)
        Apply3DAudio(playback.get());
    else
        UpdateVolume(playback.get());

    const AudioPlaybackId id = m_nextInstanceId++;
    if (m_nextInstanceId == kInvalidAudioPlaybackId)
        m_nextInstanceId = 1;
    m_instances[id] = std::move(playback);
    return id;
}

void AudioManager::SetUiClickSound(const std::string& path) {
    m_loadedAudio.erase(kUiClickLoadedId);
    if (path.empty())
        return;
    LoadSFX(kUiClickLoadedId, path);
}

void AudioManager::PlayUiClick(float volume) {
    StartInstanceFromLoaded(kUiClickLoadedId, volume, true);
}

void AudioManager::StopInstance(AudioPlaybackId id) {
    if (id == kInvalidAudioPlaybackId) return;
    auto it = m_instances.find(id);
    if (it == m_instances.end()) return;
    DestroyPlaybackVoice(it->second.get());
    m_instances.erase(it);
}

bool AudioManager::IsInstancePlaying(AudioPlaybackId id) const {
    if (id == kInvalidAudioPlaybackId) return false;
    auto it = m_instances.find(id);
    if (it == m_instances.end()) return false;
    return !VoiceHasFinished(it->second.get());
}

void AudioManager::SetInstancePosition(AudioPlaybackId id, const DirectX::XMFLOAT3& position) {
    auto it = m_instances.find(id);
    if (it == m_instances.end() || !it->second || !it->second->is3D) return;
    it->second->position = position;
    it->second->emitter.Position = position;
}

void AudioManager::SetInstanceDistanceScale(AudioPlaybackId id, float scale) {
    auto it = m_instances.find(id);
    if (it == m_instances.end() || !it->second || !it->second->is3D) return;
    if (scale < 0.0f) scale = 0.0f;
    it->second->emitter.CurveDistanceScaler = scale;
}

void AudioManager::SetInstanceVolume(AudioPlaybackId id, float volume) {
    auto it = m_instances.find(id);
    if (it == m_instances.end() || !it->second) return;
    it->second->baseVolume = Clamp01(volume);
    if (it->second->is3D) Apply3DAudio(it->second.get());
    else UpdateVolume(it->second.get());
}

void AudioManager::StopAllInstances() {
    for (auto& pair : m_instances)
        DestroyPlaybackVoice(pair.second.get());
    m_instances.clear();
}

void AudioManager::StopSceneInstances() {
    auto it = m_instances.begin();
    while (it != m_instances.end()) {
        if (it->second && it->second->persistAcrossScene) {
            ++it;
            continue;
        }
        DestroyPlaybackVoice(it->second.get());
        it = m_instances.erase(it);
    }
}

bool AudioManager::LoadBusVolumesFromProjectFile(const std::string& projectJsonPath) {
    try {
        std::ifstream file(projectJsonPath);
        if (!file.is_open())
            return false;
        nlohmann::json j;
        file >> j;
        if (!j.contains("audio") || !j["audio"].is_object())
            return false;
        const auto& audio = j["audio"];
        SetMasterVolume(audio.value("masterVolume", 1.0f));
        SetBGMVolume(audio.value("bgmVolume", 1.0f));
        SetSFXVolume(audio.value("sfxVolume", 1.0f));
        SetVoiceVolume(audio.value("voiceVolume", 1.0f));
        SetUiClickSound(audio.value("uiClickSound", std::string{}));
        return true;
    }
    catch (...) {
        return false;
    }
}

void AudioManager::TryLoadBusVolumesFromNearbyProject() {
    namespace fs = std::filesystem;
    if (LoadBusVolumesFromProjectFile("project.json"))
        return;

    wchar_t exePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0)
        return;
    const fs::path nearby = fs::path(exePath).parent_path() / "project.json";
    LoadBusVolumesFromProjectFile(nearby.string());
}

bool AudioManager::IsContentLoaded(const std::string& path) const {
    if (path.empty()) return false;
    std::error_code ec;
    const auto key = std::filesystem::absolute(PathFromUtf8(path), ec).lexically_normal().wstring();
    if (ec) return false;
    auto matches = [&](const Playback& source) {
        if (source.contentPath.empty()) return false;
        std::error_code inner;
        const auto cached = std::filesystem::absolute(PathFromUtf8(source.contentPath), inner).lexically_normal().wstring();
        return !inner && _wcsicmp(cached.c_str(), key.c_str()) == 0;
    };
    for (const auto& entry : m_loadedAudio) {
        if (entry.second && matches(*entry.second)) return true;
    }
    for (const auto& entry : m_sources) {
        if (entry.second && matches(*entry.second)) return true;
    }
    for (const auto& entry : m_instances) {
        if (entry.second && matches(*entry.second)) return true;
    }
    return false;
}

bool AudioManager::ReloadContent(const std::string& path) {
    Playback candidate{};
    if (!LoadWav(path, &candidate)) return false;
    std::error_code ec;
    const auto key = std::filesystem::absolute(PathFromUtf8(path), ec).lexically_normal().wstring();
    if (ec) return false;
    auto matches = [&](const Playback& source) {
        if (source.contentPath.empty()) return false;
        std::error_code inner;
        const auto cached = std::filesystem::absolute(PathFromUtf8(source.contentPath), inner).lexically_normal().wstring();
        return !inner && _wcsicmp(cached.c_str(), key.c_str()) == 0;
    };
    for (auto it = m_sources.begin(); it != m_sources.end();) {
        if (matches(*it->second)) { DestroyPlaybackVoice(it->second.get()); it = m_sources.erase(it); }
        else ++it;
    }
    for (auto it = m_instances.begin(); it != m_instances.end();) {
        if (matches(*it->second)) { DestroyPlaybackVoice(it->second.get()); it = m_instances.erase(it); }
        else ++it;
    }
    for (auto& entry : m_loadedAudio) {
        auto& source = *entry.second;
        if (!matches(source)) continue;
        DestroyPlaybackVoice(&source);
        source.audioData = candidate.audioData;
        source.wfx = candidate.wfx;
    }
    return true;
}
