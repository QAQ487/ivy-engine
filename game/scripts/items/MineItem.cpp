#include "MineItem.h"

#include "ChaseTestEnemyScript.h"
#include "EnemyAiScript.h"
#include "ItemWorldApi.h"
#include "PlayerScript.h"
#include "engine/physics/collider/collider.h"
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

REGISTER_SCRIPT(MineItemScript)

namespace {
    MineItemScript* g_mineItemScript = nullptr;

    constexpr const char* kMineModel = "rom/model/items/Mine.fbx";

    const XMFLOAT3 kMineScale        = {0.7f, 0.2f, 0.7f};
    const XMFLOAT3 kMineColliderHalf = {0.3f, 0.08f, 0.3f};
}

void MineItem_ResetForWorldPickup(engine::GameObject& item) {
    ScriptComponent* sc = item.GetComponent<ScriptComponent>();
    if (!sc) return;
    if (auto* script = dynamic_cast<MineItemScript*>(sc->GetScript()))
        script->ResetStateForWorldPickup(item);
}

void MineItem_ReleaseFromField(engine::GameObject& item) {
    ScriptComponent* sc = item.GetComponent<ScriptComponent>();
    if (!sc) return;
    if (auto* script = dynamic_cast<MineItemScript*>(sc->GetScript()))
        script->ReleaseFromField(item);
}

void MineItemScript::ApplyMineAppearance(engine::GameObject& self, bool armed) {
    if (auto* vc = ScriptApi_FindVisualComponent(self)) {
        vc->modelPath = kMineModel;
        vc->proxyModelPath = kMineModel;
        if (armed) {
            vc->emissionGlow = {1.0f, 0.15f, 0.1f, 1.0f};
        } else {
            vc->emissionGlow = {0.85f, 0.55f, 0.1f, 1.0f};
        }
    }
    self.SetScale(kMineScale);
    if (engine::Transform* t = ScriptApi_FindTransform(self))
        t->localScale = kMineScale;
    if (auto* box = dynamic_cast<BoxCollider*>(self.GetCollider()))
        ScriptApi_SetBoxColliderSize(*box, {
            kMineColliderHalf.x * 2.0f,
            kMineColliderHalf.y * 2.0f,
            kMineColliderHalf.z * 2.0f
        });
}

void MineItemScript::ApplyArmingAppearance(engine::GameObject& self, float armProgress) {
    if (auto* vc = ScriptApi_FindVisualComponent(self)) {
        vc->modelPath = kMineModel;
        vc->proxyModelPath = kMineModel;
        const float pulse = 0.5f + 0.5f * sinf(armProgress * 12.0f);
        vc->emissionGlow = {0.95f * pulse, 0.75f * pulse, 0.1f, 1.0f};
    }
    self.SetScale(kMineScale);
    if (engine::Transform* t = ScriptApi_FindTransform(self))
        t->localScale = kMineScale;
}

void MineItemScript::ResetStateForWorldPickup(engine::GameObject& self) {
    m_state = State::MysteryBoxWorld;
    m_armTimer = 0.0f;
    m_placedLifetimeTimer = 0.0f;
    m_detonateVisualTimer = 0.0f;
    m_mineCenter = {0.0f, 0.0f, 0.0f};
    m_placedByPlayer = nullptr;
    (void)self;
}

void MineItemScript::ReleaseFromField(engine::GameObject& self) {
    m_state = State::MysteryBoxWorld;
    m_armTimer = 0.0f;
    m_placedLifetimeTimer = 0.0f;
    m_detonateVisualTimer = 0.0f;
    m_mineCenter = {0.0f, 0.0f, 0.0f};
    m_placedByPlayer = nullptr;
    ItemWorld_DestroyRuntimeItem(self);
}

bool MineItemScript::IsWorldPickupState() const {
    return m_state == State::MysteryBoxWorld;
}

bool MineItemScript::IsHeldByBody(const GameObject& body) const {
    (void)body;
    return m_state == State::HeldByPlayer;
}

bool MineItemScript::IsBodyInPickupRange(GameObject& self,
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

bool MineItemScript::TryPickupForBody(engine::GameObject& self, GameObject& body) {
    if (m_state != State::MysteryBoxWorld || !body.IsActive())
        return false;
    if (!IsBodyInPickupRange(self, body))
        return false;

    ApplyMineAppearance(self, false);
    MoveToHeldHidePosition(self);
    m_state = State::HeldByPlayer;
    m_placedByPlayer = nullptr;
    return true;
}

bool MineItemScript::TryPlaceForBody(engine::GameObject& self,
                                     GameObject& body,
                                     float yawDegrees) {
    if (!ItemWorld_IsHeldBy(self, body))
        return false;
    if (m_state != State::HeldByPlayer)
        m_state = State::HeldByPlayer;

    XMFLOAT3 placePos = body.GetPosition();
    const XMFLOAT3 forward = GetForwardFromYaw(yawDegrees);
    placePos.x += forward.x * minePlaceDistance;
    placePos.z += forward.z * minePlaceDistance;

    XMFLOAT3 floorCenter = {};
    XMFLOAT3 floorSize = {};
    if (FindLargestGroundFootprint(floorCenter, floorSize))
        placePos.y = GetPlacedMineY(floorCenter, floorSize);

    SyncTransform(self, placePos);
    m_placedByPlayer = &body;
    m_mineCenter = placePos;
    m_armTimer = (std::max)(0.05f, mineArmDelay);
    m_placedLifetimeTimer = (std::max)(0.0f, minePlacedLifetime);
    m_state = State::PlacedArming;
    ApplyArmingAppearance(self, 0.0f);
    return true;
}

void MineItemScript::OnCreate(engine::GameObject& self) {
    g_mineItemScript = this;
    self.SetActive(true);
    m_placedByPlayer = nullptr;
    if (!ItemWorld_GetHolder(self))
        ResetStateForWorldPickup(self);
}

void MineItemScript::OnDestroy(engine::GameObject& self) {
    (void)self;
    if (g_mineItemScript == this)
        g_mineItemScript = nullptr;
}

bool MineItemScript::FindLargestGroundFootprint(XMFLOAT3& outCenter, XMFLOAT3& outSize) const {
    outCenter = {0.0f, 0.0f, 0.0f};
    outSize   = {1.0f, 1.0f, 1.0f};

    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (!scene) return false;

    float bestArea = -1.0f;
    for (GameObject* obj : scene->GetObjects()) {
        if (!obj || !obj->IsActive() || obj->GetTag() != Tag::TAG_GROUND) continue;

        Collider* collider = obj->GetCollider();
        if (!collider) continue;

        XMFLOAT3 center = {};
        XMFLOAT3 size = {1.0f, 1.0f, 1.0f};
        if (!ScriptApi_TryGetColliderFootprint(*collider, center, size))
            continue;

        const float area = size.x * size.z;
        if (area > bestArea) {
            bestArea = area;
            outCenter = center;
            outSize = size;
        }
    }

    return bestArea > 0.0f;
}

float MineItemScript::GetPlacedMineY(const XMFLOAT3& floorCenter, const XMFLOAT3& floorSize) const {
    const float floorTop = floorCenter.y + floorSize.y * 0.5f;
    const float mineHalfHeight = kMineScale.y * 0.5f;
    return floorTop + mineHalfHeight;
}

void MineItemScript::SyncTransform(engine::GameObject& self, const XMFLOAT3& worldPos) {
    self.SetPosition(worldPos);
    if (engine::Transform* t = ScriptApi_FindTransform(self))
        t->localPosition = worldPos;
}

void MineItemScript::MoveToHeldHidePosition(engine::GameObject& self) {
    XMFLOAT3 hiddenPos = self.GetPosition();
    hiddenPos.y = heldHideY;
    SyncTransform(self, hiddenPos);
}

XMFLOAT3 MineItemScript::GetForwardFromYaw(float yawDegrees) const {
    const float yawRad = XMConvertToRadians(yawDegrees);
    return {sinf(yawRad), 0.0f, cosf(yawRad)};
}

bool MineItemScript::IsWithinTriggerRadius(const XMFLOAT3& center, const XMFLOAT3& targetPos) const {
    const float dx = targetPos.x - center.x;
    const float dz = targetPos.z - center.z;
    const float radius = (std::max)(0.1f, mineTriggerRadius);
    return (dx * dx + dz * dz) <= (radius * radius);
}

bool MineItemScript::CheckForTrigger(const XMFLOAT3& center) const {
    const float radius = (std::max)(0.1f, mineTriggerRadius);

    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (!scene) return false;

    for (GameObject* obj : scene->GetObjects()) {
        if (!obj || !obj->IsActive()) continue;
        if (obj->GetTag() != Tag::TAG_PLAYER) continue;
        if (IsWithinTriggerRadius(center, obj->GetPosition()))
            return true;
    }

    if (GameObject* player = PlayerScript_GetBody()) {
        if (player->IsActive() && IsWithinTriggerRadius(center, player->GetPosition()))
            return true;
    }

    if (EnemyAi_IsAnyoneInRadius(center, radius)) return true;
    if (ChaseTestEnemy_IsAnyoneInRadius(center, radius)) return true;

    return false;
}

void MineItemScript::Detonate(const XMFLOAT3& center) {
    const float radius = (std::max)(0.1f, mineTriggerRadius);
    const float stunDuration = (std::max)(0.05f, mineStunDuration);

    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (scene) {
        for (GameObject* obj : scene->GetObjects()) {
            if (!obj || !obj->IsActive()) continue;
            if (obj->GetTag() != Tag::TAG_PLAYER) continue;
            if (!IsWithinTriggerRadius(center, obj->GetPosition())) continue;
            PlayerScript_BeginStunForBody(obj, stunDuration);
        }
    }

    if (GameObject* player = PlayerScript_GetBody()) {
        if (player->IsActive() && IsWithinTriggerRadius(center, player->GetPosition()))
            PlayerScript_BeginStunForBody(player, stunDuration);
    }

    EnemyAi_ApplyStunInRadius(center, radius, stunDuration, true);
    ChaseTestEnemy_ApplyStunInRadius(center, radius, stunDuration, true);
}

void MineItemScript::DrawArmingIndicator(const XMFLOAT3& center, float progress) const {
    const float alpha = 0.35f + 0.45f * progress;
    const float radius = (std::max)(0.1f, mineTriggerRadius) * (0.5f + 0.5f * progress);
    ScriptApi_DebugDrawSphereWire(
        {center.x, center.y + 0.1f, center.z},
        {0.0f, 0.0f, 0.0f},
        radius,
        {0.0f, 0.0f, 0.0f},
        {0.95f, 0.75f, 0.1f, alpha * 0.7f});
}

void MineItemScript::DrawArmedIndicator(const XMFLOAT3& center) const {
    const float radius = (std::max)(0.1f, mineTriggerRadius);
    ScriptApi_DebugDrawSphereWire(
        {center.x, center.y + 0.1f, center.z},
        {0.0f, 0.0f, 0.0f},
        radius,
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.15f, 0.1f, 0.65f});
}

void MineItemScript::DrawDetonationBurst(const XMFLOAT3& center, float alpha) const {
    const float radius = (std::max)(0.1f, mineTriggerRadius);
    ScriptApi_DebugDrawSphereWire(
        {center.x, center.y + 0.15f, center.z},
        {0.0f, 0.0f, 0.0f},
        radius,
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.5f, 0.1f, alpha});
}

void MineItemScript::OnUpdate(engine::GameObject& self, float dt) {
    if (!EngineGUI_IsPlaying()) return;

    GameObject* player = PlayerScript_GetBody();
    if (!player || !player->IsActive()) return;

    if (KeyLogger_IsTrigger(KK_K) && m_state != State::HeldByPlayer && m_state != State::Detonated
        && !ItemWorld_HolderHasItem(*player)) {
        ItemWorld_TryPickup(self, *player);
        return;
    }

    if (m_state == State::PlacedArming) {
        m_armTimer -= dt;
        const float armDelay = (std::max)(0.05f, mineArmDelay);
        const float progress = 1.0f - (m_armTimer / armDelay);
        ApplyArmingAppearance(self, progress);
        if (progress >= 0.08f)
            DrawArmingIndicator(m_mineCenter, progress);

        m_placedLifetimeTimer -= dt;
        if (m_armTimer <= 0.0f) {
            m_state = State::PlacedArmed;
            ApplyMineAppearance(self, true);
        }
    }
    else if (m_state == State::PlacedArmed) {
        DrawArmedIndicator(m_mineCenter);

        if (CheckForTrigger(m_mineCenter)) {
            Detonate(m_mineCenter);
            m_state = State::Detonated;
            m_detonateVisualTimer = (std::max)(0.05f, mineDetonateVisualDuration);
            MoveToHeldHidePosition(self);
        } else {
            m_placedLifetimeTimer -= dt;
            if (m_placedLifetimeTimer <= 0.0f) {
                // Consumed / expired: never ResetStateForWorldPickup here — that leaves a
                // pickable mine at hide Y, and auto-pickup scans XZ only.
                if (!ItemWorld_RequestRespawn(self))
                    ReleaseFromField(self);
            }
        }
    }
    else if (m_state == State::Detonated) {
        const float visualDuration = (std::max)(0.05f, mineDetonateVisualDuration);
        const float alpha = (std::max)(0.0f, m_detonateVisualTimer / visualDuration);
        DrawDetonationBurst(m_mineCenter, alpha);

        m_detonateVisualTimer -= dt;
        if (m_detonateVisualTimer <= 0.0f) {
            if (!ItemWorld_RequestRespawn(self))
                ReleaseFromField(self);
        }
    }
}
