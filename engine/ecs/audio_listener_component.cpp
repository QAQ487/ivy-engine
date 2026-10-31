// audio_listener_component.cpp
// ECS: audio listener component implementation.

#include "audio_listener_component.h"

#include "libraries/nlohmann/json.hpp"

using json = nlohmann::json;

std::string AudioListenerComponent::Serialize() const {
    return json::object().dump();
}

void AudioListenerComponent::Deserialize(const std::string& /*jsonStr*/) {
}
