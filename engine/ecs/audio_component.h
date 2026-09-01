#pragma once

#include "component.h"
#include "engine/audio/audio_manager.h"

#include <string>

class AudioComponent : public Component {
public:
    std::string clipPath;
    AudioType bus = AudioType::SFX;
    float volume = 1.0f;
    bool loop = false;
    bool autoPlay = true;
    bool spatial = false;
    float distanceScale = 20.0f;

    const char* GetTypeName() const override { return "Audio"; }

    void OnAttach() override;
    void OnDetach() override;
    void Update(double elapsed_time) override;
    void OnReset() override;

    std::string Serialize() const override;
    void Deserialize(const std::string& jsonStr) override;

    void Play();
    void Stop();
    bool IsPlaying() const;
    void SetClipPath(const std::string& path);
    void ApplyLiveSettings();

private:
    AudioPlaybackId m_playbackId = kInvalidAudioPlaybackId;
    bool m_hasAutoPlayed = false;

    DirectX::XMFLOAT3 OwnerPosition() const;
};
