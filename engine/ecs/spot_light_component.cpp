// spot_light_component.cpp
// ECS: spot light component implementation.

#include "spot_light_component.h"
#include "libraries/nlohmann/json.hpp"
#include <cmath>
#include <Windows.h>
#include "engine/debug_draw/debug_ofstream.h"

std::string SpotLightComponent::Serialize() const {
    nlohmann::json j;
    j["enabled"] = enabled;
    j["color"] = { color.x, color.y, color.z };
    j["intensity"] = intensity;
    j["specColor"] = { specColor.x, specColor.y, specColor.z };
    j["specPower"] = specPower;
    j["range"] = range;
    j["attenuation"] = { attenuation.x, attenuation.y, attenuation.z };
    j["innerAngleDeg"] = innerAngleDeg;
    j["outerAngleDeg"] = outerAngleDeg;
    j["castShadows"] = castShadows;
    return j.dump();
}

void SpotLightComponent::Deserialize(const std::string& jsonStr) {
    try {
        auto j = nlohmann::json::parse(jsonStr);
        enabled = j.value("enabled", true);
        intensity = j.value("intensity", 1.0f);
        specPower = j.value("specPower", 32.0f);
        range = j.value("range", 10.0f);
        innerAngleDeg = j.value("innerAngleDeg", 20.0f);
        outerAngleDeg = j.value("outerAngleDeg", 30.0f);
        castShadows = j.value("castShadows", false);

        if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 3) {
            color.x = j["color"][0].get<float>();
            color.y = j["color"][1].get<float>();
            color.z = j["color"][2].get<float>();
        }
        if (j.contains("specColor") && j["specColor"].is_array() && j["specColor"].size() >= 3) {
            specColor.x = j["specColor"][0].get<float>();
            specColor.y = j["specColor"][1].get<float>();
            specColor.z = j["specColor"][2].get<float>();
        }
        if (j.contains("attenuation") && j["attenuation"].is_array() && j["attenuation"].size() >= 3) {
            attenuation.x = j["attenuation"][0].get<float>();
            attenuation.y = j["attenuation"][1].get<float>();
            attenuation.z = j["attenuation"][2].get<float>();
        }
    } catch (...) {
        hal::DebugWriteA("[SpotLightComponent] Deserialize: JSON parse error.\n");
    }
}
