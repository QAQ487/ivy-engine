// SampleTriggerLogScript.h
// Declares SampleTriggerLogScript.

#pragma once
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"
#include <DirectXMath.h>

// Physics-events demo: reacts to trigger enter/exit and optionally raycasts.
class SampleTriggerLogScript : public Script {
public:
#define SAMPLE_TRIGGER_LOG_FIELDS(X) \
    X(float, raycastMaxDistance, 20.0f) \
    X(float, enableRaycastSmokeTest, 1.0f)

    SAMPLE_TRIGGER_LOG_FIELDS(SCRIPT_DECLARE_FIELD)
    SCRIPT_SERIALIZE_METHODS(SAMPLE_TRIGGER_LOG_FIELDS)

    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;
    void OnTriggerEnter(engine::GameObject& self, engine::GameObject* other) override;
    void OnTriggerExit(engine::GameObject& self, engine::GameObject* other) override;

private:
    DirectX::XMFLOAT3 m_baseColor{ 0.2f, 0.7f, 1.0f };
    float m_raycastCooldown = 0.0f;
};
