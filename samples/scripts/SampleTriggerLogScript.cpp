#include "SampleTriggerLogScript.h"

#include "engine/ecs/material_component.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"

#include <cstdio>

REGISTER_SCRIPT(SampleTriggerLogScript)

void SampleTriggerLogScript::OnCreate(engine::GameObject& self) {
    if (MaterialComponent* mat = ScriptApi_FindMaterialComponent(self))
        m_baseColor = mat->color;
}

void SampleTriggerLogScript::OnUpdate(engine::GameObject& self, float dt) {
    if (enableRaycastSmokeTest < 0.5f) return;

    m_raycastCooldown -= dt;
    if (m_raycastCooldown > 0.0f) return;
    m_raycastCooldown = 0.5f;

    const DirectX::XMFLOAT3 origin = self.GetPosition();
    const DirectX::XMFLOAT3 dir{ 0.0f, -1.0f, 0.0f };
    DirectX::XMFLOAT3 hitPoint{};
    DirectX::XMFLOAT3 hitNormal{};
    float hitDist = 0.0f;
    if (ScriptApi_Physics_Raycast(origin, dir, raycastMaxDistance,
            &hitPoint, &hitNormal, &hitDist)) {
        char msg[128];
        std::snprintf(msg, sizeof(msg),
            "Raycast hit dist=%.2f n=(%.1f,%.1f,%.1f)",
            hitDist, hitNormal.x, hitNormal.y, hitNormal.z);
        ScriptApi_PickupDebugSetMessage(msg, 0.6f);
    }
}

void SampleTriggerLogScript::OnTriggerEnter(engine::GameObject& self,
    engine::GameObject* other) {
    (void)other;
    if (MaterialComponent* mat = ScriptApi_FindMaterialComponent(self))
        mat->color = { 1.0f, 0.35f, 0.1f };
    ScriptApi_PickupDebugSetMessage("OnTriggerEnter", 2.0f);
}

void SampleTriggerLogScript::OnTriggerExit(engine::GameObject& self,
    engine::GameObject* other) {
    (void)other;
    if (MaterialComponent* mat = ScriptApi_FindMaterialComponent(self))
        mat->color = m_baseColor;
    ScriptApi_PickupDebugSetMessage("OnTriggerExit", 2.0f);
}
