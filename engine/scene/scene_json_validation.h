// scene_json_validation.h
// Scene: scene json validation declarations.

#pragma once
#include "libraries/nlohmann/json.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>

// Validate known fields before legacy deserializers (some swallow type errors).
// Missing/unknown fields retain their existing compatibility behaviour.
inline void SceneJson_ValidateTypes(const nlohmann::json& value,
    const nlohmann::json& defaults, const std::string& path) {
    if (defaults.is_null()) return;
    if (defaults.is_number() && value.is_number()) return;
    if (value.type() != defaults.type()) throw std::runtime_error("Invalid type: " + path);
    if (defaults.is_object()) {
        for (auto it = defaults.begin(); it != defaults.end(); ++it)
            if (value.contains(it.key())) SceneJson_ValidateTypes(value[it.key()], it.value(), path + "." + it.key());
    } else if (defaults.is_array() && !defaults.empty()) {
        for (size_t i = 0; i < value.size(); ++i)
            SceneJson_ValidateTypes(value[i], defaults[(std::min)(i, defaults.size() - 1)], path + "[]");
    }
}
