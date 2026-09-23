#pragma once
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"

// Invisible capsule mover with first-person camera (walkable camera rig).
class SampleFirstPersonScript : public Script {
public:
#define SAMPLE_FIRST_PERSON_FIELDS(X) \
    X(float, moveSpeed, 5.0f) \
    X(float, eyeHeight, 0.7f)

    SAMPLE_FIRST_PERSON_FIELDS(SCRIPT_DECLARE_FIELD)
    SCRIPT_SERIALIZE_METHODS(SAMPLE_FIRST_PERSON_FIELDS)

    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;

private:
    float m_yaw = 0.0f;
};
