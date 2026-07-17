/*****************************************************
 * File: engine/scripting/script_fields.h
 * Project: d3d11_engine
 * Description: X-macro helpers for script serialized fields.
 *
 * Define each field once in a FIELD_LIST macro, then expand it for member
 * declarations and auto-generated Serialize() / Deserialize().
 *
 * Prefab JSON only needs "scriptClass" — defaults come from the header.
 * Optional "properties" overrides are still applied when present (scenes,
 * editor saves, per-instance tuning).
 *
 * Example:
 *   #define MY_SCRIPT_FIELDS(X) \
 *       X(float, moveSpeed, 5.0f) \
 *       X(int,   count,    1)
 *
 *   class MyScript : public Script {
 *   public:
 *       MY_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)
 *       SCRIPT_SERIALIZE_METHODS(MY_SCRIPT_FIELDS)
 *   };
 *****************************************************/
#pragma once

#include "libraries/nlohmann/json.hpp"

#define SCRIPT_DECLARE_FIELD(type, name, defaultVal) type name = defaultVal;

#define SCRIPT_FIELD_TO_JSON(type, name, defaultVal) j[#name] = name;

#define SCRIPT_FIELD_FROM_JSON(type, name, defaultVal) \
    if (j.contains(#name)) name = j[#name].get<type>();

#define SCRIPT_SERIALIZE_METHODS(FIELD_LIST)                              \
    nlohmann::json Serialize() const override {                           \
        nlohmann::json j;                                                 \
        FIELD_LIST(SCRIPT_FIELD_TO_JSON)                                  \
        return j;                                                         \
    }                                                                     \
    void Deserialize(const nlohmann::json& j) override {                  \
        FIELD_LIST(SCRIPT_FIELD_FROM_JSON)                                \
    }
