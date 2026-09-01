#include "audio_component.h"

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
}

void AudioComponent::OnDetach() {
    Stop();
}

void AudioComponent::Update(double /*elapsed_time*/) {
    if (!owner || !owner->IsActive()) return;

    if (m_playbackId != kInvalidAudioPlaybackId &&
        !AudioManager::Instance().IsInstancePlaying(m_playbackId)) {
        m_playbackId = kInvalidAudioPlaybackId;
    }

    if (autoPlay && !m_hasAutoPlayed) {
        Play();
        m_hasAutoPlayed = true;
    }

    if (spatial && m_playbackId != kInvalidAudioPlaybackId)
        AudioManager::Instance().SetInstancePosition(m_playbackId, OwnerPosition());
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
    Stop();
    if (clipPath.empty()) return;

    m_playbackId = AudioManager::Instance().StartInstance(
        clipPath,
        bus,
        volume,
        loop,
        spatial,
        OwnerPosition(),
        distanceScale);
}

void AudioComponent::Stop() {
    if (m_playbackId == kInvalidAudioPlaybackId) return;
    AudioManager::Instance().StopInstance(m_playbackId);
    m_playbackId = kInvalidAudioPlaybackId;
}

bool AudioComponent::IsPlaying() const {
    if (m_playbackId == kInvalidAudioPlaybackId) return false;
    return AudioManager::Instance().IsInstancePlaying(m_playbackId);
}

void AudioComponent::SetClipPath(const std::string& path) {
    if (clipPath == path) return;
    Stop();
    clipPath = path;
    m_hasAutoPlayed = false;
}

void AudioComponent::ApplyLiveSettings() {
    if (m_playbackId == kInvalidAudioPlaybackId) return;
    auto& audio = AudioManager::Instance();
    audio.SetInstanceVolume(m_playbackId, volume);
    if (spatial)
        audio.SetInstanceDistanceScale(m_playbackId, distanceScale);
}

DirectX::XMFLOAT3 AudioComponent::OwnerPosition() const {
    if (!owner) return { 0.0f, 0.0f, 0.0f };
    if (const engine::Transform* t = owner->GetTransform())
        return t->GetWorldPosition();
    return owner->GetPosition();
}