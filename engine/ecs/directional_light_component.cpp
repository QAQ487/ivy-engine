// directional_light_component.cpp
// ECS: directional light component implementation.

#include "directional_light_component.h"
#include "engine/editor/light_gizmo.h"
#include "libraries/nlohmann/json.hpp"
#include <Windows.h>
#include "engine/debug_draw/debug_ofstream.h"

DirectX::XMFLOAT3 DirectionalLightComponent::ComputeDirectionWS(const engine::Transform* transform) {
    return LightGizmo::GetShineDirectionWS(transform);
}

std::string DirectionalLightComponent::Serialize() const {
    nlohmann::json j;
    j["enabled"] = enabled;
    j["color"] = { color.x, color.y, color.z };
    j["intensity"] = intensity;
    j["specColor"] = { specColor.x, specColor.y, specColor.z };
    j["specPower"] = specPower;
    j["castShadows"] = castShadows;
    return j.dump();
}

void DirectionalLightComponent::Deserialize(const std::string& jsonStr) {
    try {
        auto j = nlohmann::json::parse(jsonStr);
        enabled = j.value("enabled", true);
        intensity = j.value("intensity", 1.0f);
        specPower = j.value("specPower", 32.0f);
        castShadows = j.value("castShadows", true);

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
    } catch (...) {
        hal::DebugWriteA("[DirectionalLightComponent] Deserialize: JSON parse error.\n");
    }
}
