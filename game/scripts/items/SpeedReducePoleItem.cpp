#include "SpeedReducePoleItem.h"

#include "CenterPoleScript.h"
#include "EnemyAiScript.h"
#include "ItemWorldApi.h"
#include "PlayerScript.h"
#include "engine/physics/collider/collider.h"
#include "engine/ecs/collider_component.h"
#include "engine/input/key_logger.h"
#include "engine/core/engine_gui.h"
#include "engine/core/game_object.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/visual_component.h"
#include "engine/scripting/script_component.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "scene_manager.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

REGISTER_SCRIPT(SpeedReducePoleItemScript)

namespace {
    SpeedReducePoleItemScript* g_speedReduceItemScript = nullptr;

}

void SpeedReducePoleItem_ResetForWorldPickup(engine::GameObject& item) {
    ScriptComponent* sc = item.GetComponent<ScriptComponent>();
    if (!sc) return;
    if (auto* script = dynamic_cast<SpeedReducePoleItemScript*>(sc->GetScript()))
        script->ResetStateForWorldPickup(item);
}

void SpeedReducePoleItem_ReleaseFromField(engine::GameObject& item) {
    ScriptComponent* sc = item.GetComponent<ScriptComponent>();
    if (!sc) return;
    if (auto* script = dynamic_cast<SpeedReducePoleItemScript*>(sc->GetScript()))
        script->ReleaseFromField(item);
}

void SpeedReducePoleItemScript::ApplyPoleAppearance(engine::GameObject& self) {
    if (auto* vc = ScriptApi_FindVisualComponent(self)) {
        vc->emissionGlow = {0.35f, 0.15f, 0.95f, 1.0f};
    }
    if (ColliderComponent* cc = ScriptApi_FindColliderComponent(self))
        ScriptApi_ApplyColliderComponent(*cc);
}

void SpeedReducePoleItemScript::ResetStateForWorldPickup(engine::GameObject& self) {
    ClearPlacerZoneEffects();
    m_state = State::MysteryBoxWorld;
    m_placedLifetimeTimer = 0.0f;
    m_wasPlacerInZone = false;
    m_zoneCenter = {0.0f, 0.0f, 0.0f};
    m_placedByPlayer = nullptr;
    SyncMagneticPoleRegistration(self);
    (void)self;
}

void SpeedReducePoleItemScript::ReleaseFromField(engine::GameObject& self) {
    ClearPlacerZoneEffects();
    UnregisterMagneticPole();
    m_state = State::MysteryBoxWorld;
    m_placedLifetimeTimer = 0.0f;
    m_wasPlacerInZone = false;
    m_zoneCenter = {0.0f, 0.0f, 0.0f};
    m_placedByPlayer = nullptr;
    ItemWorld_DestroyRuntimeItem(self);
}

bool SpeedReducePoleItemScript::IsWorldPickupState() const {
    return m_state == State::MysteryBoxWorld;
}

bool SpeedReducePoleItemScript::IsHeldByBody(const GameObject& body) const {
    (void)body;
    return m_state == State::HeldByPlayer;
}

bool SpeedReducePoleItemScript::IsBodyInPickupRange(GameObject& self,
                                                    GameObject& body) const {
    const float pickupRadius = (std::max)(0.1f, itemPickupRadius);
    if (Collider* bodyCol = body.GetCollider()) {
        if (Collider* itemCol = self.GetCollider()) {
            if (bodyCol->CheckCollision(itemCol))
                return true;
        }
    }

    const XMFLOAT3 itemPos = self.GetPosition();
    const XMFLOAT3 bodyPos = body.GetPosition();
    const float dx = bodyPos.x - itemPos.x;
    const float dz = bodyPos.z - itemPos.z;
    return (dx * dx + dz * dz) <= (pickupRadius * pickupRadius);
}

bool SpeedReducePoleItemScript::TryPickupForBody(engine::GameObject& self, GameObject& body) {
    if (m_state != State::MysteryBoxWorld || !body.IsActive())
        return false;
    if (!IsBodyInPickupRange(self, body))
        return false;

    ApplyPoleAppearance(self);
    MoveToHeldHidePosition(self);
    m_state = State::HeldByPlayer;
    m_placedByPlayer = nullptr;
    return true;
}

bool SpeedReducePoleItemScript::TryPlaceForBody(engine::GameObject& self,
                                                GameObject& body,
                                                float yawDegrees) {
    if (!ItemWorld_IsHeldBy(self, body))
        return false;
    if (m_state != State::HeldByPlayer)
        m_state = State::HeldByPlayer;

    XMFLOAT3 placePos = body.GetPosition();
    const XMFLOAT3 forward = GetForwardFromYaw(yawDegrees);
    placePos.x += forward.x * polePlaceDistance;
    placePos.z += forward.z * polePlaceDistance;
	placePos.y = 0.5f;

    SyncTransform(self, placePos);
    m_placedByPlayer = &body;
    m_zoneCenter = placePos;
    m_placedLifetimeTimer = (std::max)(0.0f, polePlacedLifetime);
    m_wasPlacerInZone = IsPlacerInsideZone(m_zoneCenter);
    m_state = State::PlacedActive;
    ApplyPoleAppearance(self);
    SyncMagneticPoleRegistration(self);
    if (EnemyAi_FindByPhysicsProxy(&body))
        TickZoneEffectsForOpponents(m_zoneCenter);
    else
        TickPlacedZoneEffects(m_zoneCenter);
    return true;
}

void SpeedReducePoleItemScript::TickZoneEffectsForOpponents(const XMFLOAT3& zoneCenter) {
    const float radius = (std::max)(0.1f, slowZoneRadius);
    const float scale = (std::max)(0.05f, (std::min)(1.0f, slowZoneMoveSpeedScale));

    auto applySlowToBody = [&](GameObject* body) {
        if (!body || !body->IsActive()) return;
        const float dx = body->GetPosition().x - zoneCenter.x;
        const float dz = body->GetPosition().z - zoneCenter.z;
        if ((dx * dx + dz * dz) > radius * radius) return;

        PlayerScript_SetMoveSpeedScaleForBody(body, scale);
        PlayerScript_SetKnockbackGroundStickForBody(body, true);
    };

    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (scene) {
        for (GameObject* obj : scene->GetObjects()) {
            if (!obj || !obj->IsActive() || obj->GetTag() != Tag::TAG_PLAYER) continue;
            if (obj == m_placedByPlayer) continue;
            applySlowToBody(obj);
        }
    }

    if (GameObject* player = PlayerScript_GetBody()) {
        if (player != m_placedByPlayer)
            applySlowToBody(player);
    }
}

bool SpeedReducePoleItemScript::IsMagnetActiveOnField() const {
    return m_state == State::PlacedActive;
}

void SpeedReducePoleItemScript::UnregisterMagneticPole() {
    if (m_magnetRegistryIndex >= 0) {
        if (engine::GameObject* node = CenterPole_GetRegistryNode(m_magnetRegistryIndex))
            CenterPole_ClearOrbitIfNode(node);
        CenterPole_UnregisterExternalPole(m_magnetRegistryIndex);
        m_magnetRegistryIndex = -1;
    }
}

void SpeedReducePoleItemScript::SyncMagneticPoleRegistration(engine::GameObject& self) {
    if (!IsMagnetActiveOnField()) {
        UnregisterMagneticPole();
        return;
    }

    const XMFLOAT3 pos = self.GetPosition();
    const XMFLOAT3 scale = self.GetScale();
    if (m_magnetRegistryIndex < 0) {
        CenterPole_RegisterExternalPole(self, scale, &m_magnetRegistryIndex);
    } else {
        CenterPole_UpdateExternalPole(m_magnetRegistryIndex, pos, scale);
    }
}

void SpeedReducePoleItemScript::ClearPlacerZoneEffects() {
    if (!m_placedByPlayer) return;
    PlayerScript_SetMoveSpeedScaleForBody(m_placedByPlayer, 1.0f);
    PlayerScript_SetKnockbackGroundStickForBody(m_placedByPlayer, false);
}

float SpeedReducePoleItemScript::RetainedKnockbackScale() const {
    const float reduction = (std::max)(0.0f, (std::min)(1.0f, zoneKnockbackReduction));
    return 1.0f - reduction;
}

void SpeedReducePoleItemScript::OnCreate(engine::GameObject& self) {
    g_speedReduceItemScript = this;
    self.SetActive(true);
    m_magnetRegistryIndex = -1;
    m_placedByPlayer = nullptr;
    m_wasPlacerInZone = false;
    if (!ItemWorld_GetHolder(self))
        ResetStateForWorldPickup(self);
}

void SpeedReducePoleItemScript::OnDestroy(engine::GameObject& self) {
    (void)self;
    ClearPlacerZoneEffects();
    UnregisterMagneticPole();
    if (g_speedReduceItemScript == this)
        g_speedReduceItemScript = nullptr;
}

bool SpeedReducePoleItemScript::FindLargestGroundBox(XMFLOAT3& outCenter, XMFLOAT3& outSize) const {
    outCenter = {0.0f, 0.0f, 0.0f};
    outSize   = {1.0f, 1.0f, 1.0f};

    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (!scene) return false;

    float bestArea = -1.0f;
    for (GameObject* obj : scene->GetObjects()) {
        if (!obj || !obj->IsActive() || obj->GetTag() != Tag::TAG_GROUND) continue;
        auto* box = dynamic_cast<BoxCollider*>(obj->GetCollider());
        if (!box) continue;

        const XMFLOAT3 size = ScriptApi_GetBoxColliderSize(*box);
        const float area = size.x * size.z;
        if (area > bestArea) {
            bestArea = area;
            outCenter = ScriptApi_GetColliderWorldCenter(*box);
            outSize = size;
        }
    }

    return bestArea > 0.0f;
}

void SpeedReducePoleItemScript::SyncTransform(engine::GameObject& self, const XMFLOAT3& worldPos) {
    self.SetPosition(worldPos);
    if (engine::Transform* t = ScriptApi_FindTransform(self))
        t->localPosition = worldPos;
}

void SpeedReducePoleItemScript::MoveToHeldHidePosition(engine::GameObject& self) {
    UnregisterMagneticPole();
    XMFLOAT3 hiddenPos = self.GetPosition();
    hiddenPos.y = heldHideY;
    SyncTransform(self, hiddenPos);
}

XMFLOAT3 SpeedReducePoleItemScript::GetForwardFromYaw(float yawDegrees) const {
    const float yawRad = XMConvertToRadians(yawDegrees);
    return {sinf(yawRad), 0.0f, cosf(yawRad)};
}

bool SpeedReducePoleItemScript::IsPlacerInsideZone(const XMFLOAT3& zoneCenter) const {
    if (!m_placedByPlayer || !m_placedByPlayer->IsActive()) return false;

    const XMFLOAT3 playerPos = m_placedByPlayer->GetPosition();
    const float dx = playerPos.x - zoneCenter.x;
    const float dz = playerPos.z - zoneCenter.z;
    const float radius = (std::max)(0.1f, slowZoneRadius);
    return (dx * dx + dz * dz) <= (radius * radius);
}

void SpeedReducePoleItemScript::TickPlacedZoneEffects(const XMFLOAT3& zoneCenter) {
    if (!m_placedByPlayer) return;

    const bool inZone = IsPlacerInsideZone(zoneCenter);
    if (inZone) {
        const float scale = (std::max)(0.05f, (std::min)(1.0f, slowZoneMoveSpeedScale));
        PlayerScript_SetMoveSpeedScaleForBody(m_placedByPlayer, scale);
        PlayerScript_SetKnockbackGroundStickForBody(m_placedByPlayer, true);

        const bool enteredZone = inZone && !m_wasPlacerInZone;
        if (enteredZone && PlayerScript_IsKnockbackActiveForBody(m_placedByPlayer))
            PlayerScript_ScaleKnockbackVelocityForBody(m_placedByPlayer, RetainedKnockbackScale());
    } else {
        PlayerScript_SetMoveSpeedScaleForBody(m_placedByPlayer, 1.0f);
        PlayerScript_SetKnockbackGroundStickForBody(m_placedByPlayer, false);
    }

    m_wasPlacerInZone = inZone;
}

void SpeedReducePoleItemScript::DrawSlowZone(const XMFLOAT3& center, float alpha) const {
    const float radius = (std::max)(0.1f, slowZoneRadius);
    const XMFLOAT3 drawCenter = {center.x, center.y + 0.75f, center.z};

    ScriptApi_DebugDrawSphereWire(
        drawCenter,
        {0.0f, 0.0f, 0.0f},
        radius,
        {0.0f, 0.0f, 0.0f},
        {0.55f, 0.2f, 1.0f, alpha});
}

void SpeedReducePoleItemScript::RunKnockbackTest(GameObject& player) {
    const XMFLOAT3 forward = GetForwardFromYaw(player.GetRotation().y);
    const float impulse = (std::max)(0.0f, testKnockbackImpulse);
    const float lift = (std::max)(0.0f, testKnockbackLift);
    PlayerScript_ApplyKnockbackImpulseForBody(
        &player,
        {forward.x * impulse, lift, forward.z * impulse},
        (std::max)(0.05f, testKnockbackDuration));

    if (m_state == State::PlacedActive
        && m_placedByPlayer == &player
        && IsPlacerInsideZone(m_zoneCenter)) {
        PlayerScript_ScaleKnockbackVelocityForBody(&player, RetainedKnockbackScale());
        PlayerScript_SetKnockbackGroundStickForBody(&player, true);
    }
}

void SpeedReducePoleItemScript::OnUpdate(engine::GameObject& self, float dt) {
    if (!EngineGUI_IsPlaying()) return;

    GameObject* player = PlayerScript_GetBody();
    if (!player || !player->IsActive()) return;

    if (KeyLogger_IsTrigger(KK_J)) {
        RunKnockbackTest(*player);
        return;
    }

    if (KeyLogger_IsTrigger(KK_K) && m_state != State::HeldByPlayer
        && !ItemWorld_HolderHasItem(*player)) {
        ItemWorld_TryPickup(self, *player);
        return;
    }

    if (m_state == State::PlacedActive) {
        DrawSlowZone(m_zoneCenter, 0.55f);
        if (m_placedByPlayer && EnemyAi_FindByPhysicsProxy(m_placedByPlayer))
            TickZoneEffectsForOpponents(m_zoneCenter);
        else
            TickPlacedZoneEffects(m_zoneCenter);

        m_placedLifetimeTimer -= dt;
        if (m_placedLifetimeTimer <= 0.0f) {
            ClearPlacerZoneEffects();
            UnregisterMagneticPole();
            if (!ItemWorld_RequestRespawn(self))
                ResetStateForWorldPickup(self);
        }
    }

    SyncMagneticPoleRegistration(self);
}
