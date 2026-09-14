#pragma once
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"

// Minimal WASD / stick mover with third-person camera follow.
class SamplePlayerScript : public Script {
public:
#define SAMPLE_PLAYER_FIELDS(X) \
    X(float, moveSpeed, 6.0f)

    SAMPLE_PLAYER_FIELDS(SCRIPT_DECLARE_FIELD)
    SCRIPT_SERIALIZE_METHODS(SAMPLE_PLAYER_FIELDS)

    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;

private:
    float m_yaw = 0.0f;
};
