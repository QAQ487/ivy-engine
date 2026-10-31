// audio_component.cpp
// ECS audio source component.

#include "audio_component.h"
#include "audio_system.h"

#include "engine/ecs/game_object.h"
#include "engine/ecs/transform.h"

#include "libraries/nlohmann/json.hpp"

#include <Windows.h>

using json = nlohmann::json;

namespace {

const char* BusToString(AudioType type) {
    switch (type) {
    case AudioType::BGM: return "BGM";
    case AudioType::VOICE: return "VOICE";
    case AudioType::SFX:
    default: return "SFX";
    }
}

AudioType BusFromString(const std::string& value) {
    if (value == "BGM") return AudioType::BGM;
    if (value == "VOICE") return AudioType::VOICE;
    return AudioType::SFX;
}

} // namespace

void AudioComponent::OnAttach() {
    AudioSystem_Register(this);
}

void AudioComponent::OnDetach() {
    AudioSystem_Unregister(this);
    ApplyStopImmediate();
    m_cmdPlay = m_cmdStop = m_cmdSyncPosition = false;
    m_cmdCheckFinished = m_cmdApplySettings = false;
}

void AudioComponent::Update(double /*elapsed_time*/) {
    if (!owner || !owner->IsActive()) return;

    m_cmdCheckFinished = true;

    if (autoPlay && !m_hasAutoPlayed) {
        Play();
        m_hasAutoPlayed = true;
    }

    if (spatial)
        m_cmdSyncPosition = true;
}

void AudioComponent::OnReset() {
    Stop();
    m_hasAutoPlayed = false;
}

std::string AudioComponent::Serialize() const {
    json j;
    j["clipPath"] = clipPath;
    j["bus"] = BusToString(bus);
    j["volume"] = volume;
    j["loop"] = loop;
    j["autoPlay"] = autoPlay;
    j["spatial"] = spatial;
    j["distanceScale"] = distanceScale;
    return j.dump();
}

void AudioComponent::Deserialize(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        clipPath = j.value("clipPath", "");
        bus = BusFromString(j.value("bus", "SFX"));
        volume = j.value("volume", 1.0f);
        loop = j.value("loop", false);
        autoPlay = j.value("autoPlay", true);
        spatial = j.value("spatial", false);
        distanceScale = j.value("distanceScale", 20.0f);
        Stop();
        m_hasAutoPlayed = false;
    }
    catch (...) {
        OutputDebugStringA("[AudioComponent] Deserialize failed.\n");
    }
}

void AudioComponent::Play() {
    m_cmdStop = true;
    m_cmdPlay = true;
    if (!GameObject::IsInsideUpdate())
        FlushCommands();
}

void AudioComponent::Stop() {
    m_cmdPlay = false;
    m_cmdStop = true;
    if (!GameObject::IsInsideUpdate())
        FlushCommands();
}

bool AudioComponent::IsPlaying() const {
    if (m_cmdPlay && !m_cmdStop)
        return true;
    if (m_playbackId == kInvalidAudioPlaybackId)
        return false;
    return AudioManager::Instance().IsInstancePlaying(m_playbackId);
}

void AudioComponent::SetClipPath(const std::string& path) {
    if (clipPath == path) return;
    Stop();
    clipPath = path;
    m_hasAutoPlayed = false;
}

void AudioComponent::ApplyLiveSettings() {
    m_cmdApplySettings = true;
    if (!GameObject::IsInsideUpdate())
        FlushCommands();
}

void AudioComponent::ApplyStopImmediate() {
    if (m_playbackId == kInvalidAudioPlaybackId) return;
    AudioManager::Instance().StopInstance(m_playbackId);
    m_playbackId = kInvalidAudioPlaybackId;
}

void AudioComponent::FlushCommands() {
    auto& audio = AudioManager::Instance();

    if (m_cmdStop) {
        ApplyStopImmediate();
        m_cmdStop = false;
    }

    if (m_cmdPlay) {
        m_cmdPlay = false;
        if (!clipPath.empty()) {
            m_playbackId = audio.StartInstance(
                clipPath,
                bus,
                volume,
                loop,
                spatial,
                OwnerPosition(),
                distanceScale);
        }
    }

    if (m_cmdCheckFinished) {
        m_cmdCheckFinished = false;
        if (m_playbackId != kInvalidAudioPlaybackId &&
            !audio.IsInstancePlaying(m_playbackId)) {
            m_playbackId = kInvalidAudioPlaybackId;
        }
    }

    if (m_cmdApplySettings) {
        m_cmdApplySettings = false;
        if (m_playbackId != kInvalidAudioPlaybackId) {
            audio.SetInstanceVolume(m_playbackId, volume);
            if (spatial)
                audio.SetInstanceDistanceScale(m_playbackId, distanceScale);
        }
    }

    if (m_cmdSyncPosition) {
        m_cmdSyncPosition = false;
        if (spatial && m_playbackId != kInvalidAudioPlaybackId)
            audio.SetInstancePosition(m_playbackId, OwnerPosition());
    }
}

DirectX::XMFLOAT3 AudioComponent::OwnerPosition() const {
    if (!owner) return { 0.0f, 0.0f, 0.0f };
    if (const engine::Transform* t = owner->GetTransform())
        return t->GetWorldPosition();
    return owner->GetPosition();
}
