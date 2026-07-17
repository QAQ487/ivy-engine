#include "ArrowStickerItem.h"

#include "ChaseTestEnemyScript.h"
#include "EnemyAiScript.h"
#include "GameMode.h"
#include "ItemWorldApi.h"
#include "PlayerScript.h"
#include "engine/physics/collider/collider.h"
#include "engine/input/key_logger.h"
#include "engine/core/engine_gui.h"
#include "engine/core/game_object.h"
#include "engine/physics/core/rigidbody.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/visual_component.h"
#include "engine/ecs/billboard_component.h"
#include "engine/scripting/script_component.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "scene_manager.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

REGISTER_SCRIPT(ArrowStickerItemScript)

namespace {
    constexpr const char* kStickerTexturePath = "rom/texture/arrow_sticker.png";
    constexpr float kStickerBillboardSize = 1.2f;
    constexpr float kStickerSurfaceOffset = 0.01f;

    const XMFLOAT3 kPickupScale = {0.55f, 0.15f, 0.55f};
    const XMFLOAT3 kPickupColliderHalf = {0.35f, 0.08f, 0.35f};
    const XMFLOAT3 kPlacedColliderHalf = {0.5f, 0.02f, 0.5f};
}

void ArrowStickerItem_ResetForWorldPickup(engine::GameObject& item) {
    ScriptComponent* sc = item.GetComponent<ScriptComponent>();
    if (!sc) return;
    if (auto* script = dynamic_cast<ArrowStickerItemScript*>(sc->GetScript()))
        script->ResetStateForWorldPickup(item);
}

void ArrowStickerItem_ReleaseFromField(engine::GameObject& item) {
    ScriptComponent* sc = item.GetComponent<ScriptComponent>();
    if (!sc) return;
    if (auto* script = dynamic_cast<ArrowStickerItemScript*>(sc->GetScript()))
        script->ReleaseFromField(item);
}

void ArrowStickerItemScript::SyncBillboardVisual(engine::GameObject& self,
                                               float yawDegrees,
                                               float alpha,
                                               bool visible,
                                               bool groundAligned) {
    if (auto* bc = self.GetComponent<BillboardComponent>()) {
        if (!visible) {
            bc->enabled = false;
            return;
        }
        if (bc->texturePath.empty())
            bc->texturePath = kStickerTexturePath;
        bc->enabled = true;
        bc->sizeX = kStickerBillboardSize;
        bc->sizeY = kStickerBillboardSize;
        bc->color = {1.0f, 1.0f, 1.0f, alpha};
        bc->rotation = XMConvertToRadians(yawDegrees);
        // false = camera-facing (vertical); true = flat on XZ floor
        bc->groundAligned = groundAligned;
    }
}

void ArrowStickerItemScript::CachePrefabVisualPaths(GameObject& self) {
    if (!m_prefabModelPath.empty()) return;
    if (auto* vc = ScriptApi_FindVisualComponent(self)) {
        if (vc->modelPath.empty()) return;
        m_prefabModelPath = vc->modelPath;
        m_prefabProxyModelPath = vc->proxyModelPath.empty()
            ? vc->modelPath
            : vc->proxyModelPath;
    }
}

void ArrowStickerItemScript::ApplyWorldPickupAppearance(engine::GameObject& self) {
    CachePrefabVisualPaths(self);

    // Training mode: show the sticker billboard for pickup (no FBX box mesh).
    if (GameMode_IsTraining()) {
        if (auto* vc = ScriptApi_FindVisualComponent(self)) {
            vc->modelPath.clear();
            vc->proxyModelPath.clear();
            vc->emissionGlow = {0.0f, 0.0f, 0.0f, 0.0f};
        }
        // Vertical camera-facing sticker (not flat). Billboards already render
        // double-sided (no cull), so both faces are visible.
        SyncBillboardVisual(self, 0.0f, 0.92f, true, /*groundAligned=*/false);
        self.SetScale({1.0f, 1.0f, 1.0f});
        if (engine::Transform* t = ScriptApi_FindTransform(self))
            t->localScale = {1.0f, 1.0f, 1.0f};
    } else {
        if (auto* vc = ScriptApi_FindVisualComponent(self)) {
            if (!m_prefabModelPath.empty()) {
                vc->modelPath = m_prefabModelPath;
                vc->proxyModelPath = m_prefabProxyModelPath;
            }
            vc->emissionGlow = {0.2f, 0.85f, 1.0f, 1.0f};
        }
        SyncBillboardVisual(self, 0.0f, 0.0f, false);
        self.SetScale(kPickupScale);
        if (engine::Transform* t = ScriptApi_FindTransform(self))
            t->localScale = kPickupScale;
    }

    if (auto* box = dynamic_cast<BoxCollider*>(self.GetCollider()))
        ScriptApi_SetBoxColliderSize(*box, {
            kPickupColliderHalf.x * 2.0f,
            kPickupColliderHalf.y * 2.0f,
            kPickupColliderHalf.z * 2.0f
        });
}

void ArrowStickerItemScript::ApplyPlacedCollider(engine::GameObject& self) {
    self.SetScale({1.0f, 1.0f, 1.0f});
    if (engine::Transform* t = ScriptApi_FindTransform(self))
        t->localScale = {1.0f, 1.0f, 1.0f};
    if (auto* vc = ScriptApi_FindVisualComponent(self)) {
        vc->modelPath.clear();
        vc->proxyModelPath.clear();
        vc->emissionGlow = {0.0f, 0.0f, 0.0f, 0.0f};
    }
    if (auto* box = dynamic_cast<BoxCollider*>(self.GetCollider()))
        ScriptApi_SetBoxColliderSize(*box, {
            kPlacedColliderHalf.x * 2.0f,
            kPlacedColliderHalf.y * 2.0f,
            kPlacedColliderHalf.z * 2.0f
        });
}

void ArrowStickerItemScript::ResetStateForWorldPickup(engine::GameObject& self) {
    m_state = State::MysteryBoxWorld;
    m_lifetimeTimer = 0.0f;
    m_triggerFlashTimer = 0.0f;
    m_placedYawDegrees = 0.0f;
    m_stickerCenter = {0.0f, 0.0f, 0.0f};
    m_boostDirection = {0.0f, 0.0f, 1.0f};
    m_placedByPlayer = nullptr;
    m_bodyCooldownTimers.clear();
    ApplyWorldPickupAppearance(self);
    (void)self;
}

void ArrowStickerItemScript::ReleaseFromField(engine::GameObject& self) {
    m_state = State::MysteryBoxWorld;
    m_lifetimeTimer = 0.0f;
    m_triggerFlashTimer = 0.0f;
    m_placedYawDegrees = 0.0f;
    m_stickerCenter = {0.0f, 0.0f, 0.0f};
    m_boostDirection = {0.0f, 0.0f, 1.0f};
    m_placedByPlayer = nullptr;
    m_bodyCooldownTimers.clear();
    ItemWorld_DestroyRuntimeItem(self);
}

bool ArrowStickerItemScript::IsWorldPickupState() const {
    return m_state == State::MysteryBoxWorld;
}

bool ArrowStickerItemScript::IsHeldByBody(const GameObject& body) const {
    (void)body;
    return m_state == State::HeldByPlayer;
}

bool ArrowStickerItemScript::IsBodyInPickupRange(GameObject& self, GameObject& body) const {
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

bool ArrowStickerItemScript::TryPickupForBody(engine::GameObject& self, GameObject& body) {
    if (m_state != State::MysteryBoxWorld || !body.IsActive())
        return false;
    if (!IsBodyInPickupRange(self, body))
        return false;

    ApplyWorldPickupAppearance(self);
    MoveToHeldHidePosition(self);
    m_state = State::HeldByPlayer;
    m_placedByPlayer = nullptr;
    return true;
}

bool ArrowStickerItemScript::TryPlaceForBody(engine::GameObject& self,
                                           GameObject& body,
                                           float yawDegrees) {
    if (!ItemWorld_IsHeldBy(self, body))
        return false;
    if (m_state != State::HeldByPlayer)
        m_state = State::HeldByPlayer;

    XMFLOAT3 placePos = body.GetPosition();
    const XMFLOAT3 forward = GetForwardFromYaw(yawDegrees);
    placePos.x += forward.x * stickerPlaceDistance;
    placePos.z += forward.z * stickerPlaceDistance;

    XMFLOAT3 floorCenter = {};
    XMFLOAT3 floorSize = {};
    if (FindLargestGroundFootprint(floorCenter, floorSize))
        placePos.y = GetPlacedStickerY(floorCenter, floorSize);

    float dirLenSq = forward.x * forward.x + forward.z * forward.z;
    if (dirLenSq > 0.0001f) {
        const float invLen = 1.0f / sqrtf(dirLenSq);
        m_boostDirection = {forward.x * invLen, 0.0f, forward.z * invLen};
    } else {
        m_boostDirection = {0.0f, 0.0f, 1.0f};
    }

    m_placedYawDegrees = yawDegrees;
    SyncTransform(self, placePos);
    m_placedByPlayer = &body;
    m_stickerCenter = placePos;
    m_lifetimeTimer = (std::max)(0.0f, stickerLifetime);
    m_bodyCooldownTimers.clear();
    m_state = State::PlacedActive;
    ApplyPlacedCollider(self);
    return true;
}

void ArrowStickerItemScript::OnCreate(engine::GameObject& self) {
    self.SetActive(true);
    m_placedByPlayer = nullptr;
    CachePrefabVisualPaths(self);
    SyncBillboardVisual(self, 0.0f, 0.0f, false);
    if (!ItemWorld_GetHolder(self))
        ResetStateForWorldPickup(self);
}

void ArrowStickerItemScript::OnDestroy(engine::GameObject& self) {
    (void)self;
}

bool ArrowStickerItemScript::FindLargestGroundFootprint(XMFLOAT3& outCenter, XMFLOAT3& outSize) const {
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

float ArrowStickerItemScript::GetPlacedStickerY(const XMFLOAT3& floorCenter,
                                                const XMFLOAT3& floorSize) const {
    const float floorTop = floorCenter.y + floorSize.y * 0.5f;
    return floorTop + (std::max)(0.0f, stickerPlaceHeight);
}

void ArrowStickerItemScript::SyncTransform(engine::GameObject& self, const XMFLOAT3& worldPos) {
    self.SetPosition(worldPos);
    if (engine::Transform* t = ScriptApi_FindTransform(self))
        t->localPosition = worldPos;
}

void ArrowStickerItemScript::MoveToHeldHidePosition(engine::GameObject& self) {
    XMFLOAT3 hiddenPos = self.GetPosition();
    hiddenPos.y = heldHideY;
    SyncTransform(self, hiddenPos);
}

XMFLOAT3 ArrowStickerItemScript::GetForwardFromYaw(float yawDegrees) const {
    const float yawRad = XMConvertToRadians(yawDegrees);
    return {sinf(yawRad), 0.0f, cosf(yawRad)};
}

bool ArrowStickerItemScript::IsWithinTriggerRadius(const XMFLOAT3& center,
                                                   const XMFLOAT3& targetPos) const {
    const float dx = targetPos.x - center.x;
    const float dz = targetPos.z - center.z;
    const float radius = (std::max)(0.1f, stickerTriggerRadius);
    return (dx * dx + dz * dz) <= (radius * radius);
}

void ArrowStickerItemScript::TickBodyCooldowns(float dt) {
    for (auto it = m_bodyCooldownTimers.begin(); it != m_bodyCooldownTimers.end(); ) {
        it->second -= dt;
        if (it->second <= 0.0f)
            it = m_bodyCooldownTimers.erase(it);
        else
            ++it;
    }
}

void ArrowStickerItemScript::ApplyDirectionalBoost(GameObject& body) {
    if (!body.IsActive() || !body.GetCollider()) return;

    const float impulse = (std::max)(0.0f, stickerBoostImpulse);
    const float lift = (std::max)(0.0f, stickerBoostLift);
    const float duration = (std::max)(0.05f, stickerKnockbackDuration);
    const XMFLOAT3 velocityChange = {
        m_boostDirection.x * impulse,
        lift,
        m_boostDirection.z * impulse
    };

    if (GameObject* localPlayer = PlayerScript_GetBody()) {
        if (localPlayer == &body) {
            PlayerScript_BeginKnockbackForBody(localPlayer, duration);
            Rigidbody& rb = body.GetRigidbody();
            if (!rb.IsKinematic() && rb.GetMass() > 0.0f)
                rb.AddForce(velocityChange, ForceMode::VelocityChange);
            return;
        }
    }

    if (EnemyAi_FindByPhysicsProxy(&body)) {
        EnemyAi_ApplyDirectionalImpulseToProxy(
            &body, m_boostDirection, impulse, lift, duration);
        return;
    }

    if (ChaseTestEnemy_FindByPhysicsProxy(&body)) {
        ChaseTestEnemy_ApplyKnockbackToProxy(&body, velocityChange, duration, 1.0f / 60.0f);
        return;
    }

    Rigidbody& rb = body.GetRigidbody();
    if (rb.IsKinematic() || rb.GetMass() <= 0.0f) return;
    rb.AddForce(velocityChange, ForceMode::VelocityChange);
}

void ArrowStickerItemScript::TryTriggerForBody(GameObject* body) {
    if (!body || !body->IsActive()) return;
    if (!IsWithinTriggerRadius(m_stickerCenter, body->GetPosition())) return;

    const auto it = m_bodyCooldownTimers.find(body);
    if (it != m_bodyCooldownTimers.end() && it->second > 0.0f)
        return;

    ApplyDirectionalBoost(*body);
    m_bodyCooldownTimers[body] = (std::max)(0.0f, stickerPerBodyCooldown);
    m_triggerFlashTimer = 0.15f;
}

void ArrowStickerItemScript::TickTriggers(float dt) {
    (void)dt;
    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (scene) {
        for (GameObject* obj : scene->GetObjects()) {
            if (!obj || !obj->IsActive()) continue;
            if (obj->GetTag() != Tag::TAG_PLAYER) continue;
            TryTriggerForBody(obj);
        }
    }

    if (GameObject* player = PlayerScript_GetBody())
        TryTriggerForBody(player);

    for (EnemyAiScript* ai : EnemyAi_GetActiveScripts()) {
        if (!ai || ai->IsRespawning()) continue;
        GameObject* proxy = EnemyAi_GetPhysicsProxy(ai);
        TryTriggerForBody(proxy);
    }

    for (ChaseTestEnemyScript* chase : ChaseTestEnemy_GetActiveScripts()) {
        if (!chase) continue;
        TryTriggerForBody(ChaseTestEnemy_GetPhysicsProxy(chase));
    }
}

void ArrowStickerItemScript::OnUpdate(engine::GameObject& self, float dt) {
    if (!EngineGUI_IsPlaying()) return;

    GameObject* player = PlayerScript_GetBody();
    if (!player || !player->IsActive()) return;

    if (KeyLogger_IsTrigger(KK_K) && m_state != State::HeldByPlayer
        && !ItemWorld_HolderHasItem(*player)) {
        ItemWorld_TryPickup(self, *player);
        return;
    }

    if (m_state == State::PlacedActive) {
        TickBodyCooldowns(dt);
        TickTriggers(dt);

        m_lifetimeTimer -= dt;
        if (m_triggerFlashTimer > 0.0f)
            m_triggerFlashTimer -= dt;

        const float alpha = (m_triggerFlashTimer > 0.0f) ? 1.0f : 0.92f;
        XMFLOAT3 drawPos = m_stickerCenter;
        drawPos.y += kStickerSurfaceOffset;
        SyncTransform(self, drawPos);
        SyncBillboardVisual(self, m_placedYawDegrees, alpha, true);

        if (m_triggerFlashTimer > 0.0f) {
            ScriptApi_BillboardAdd(
                {m_stickerCenter.x, m_stickerCenter.y + 0.01f, m_stickerCenter.z},
                {1.35f, 1.35f},
                {0.3f, 0.95f, 1.0f, 0.55f},
                nullptr,
                XMConvertToRadians(m_placedYawDegrees),
                true);
        }

        const float radius = (std::max)(0.1f, stickerTriggerRadius);
        ScriptApi_DebugDrawSphereWire(
            {m_stickerCenter.x, m_stickerCenter.y + 0.1f, m_stickerCenter.z},
            {0.0f, 0.0f, 0.0f},
            radius,
            {0.0f, 0.0f, 0.0f},
            {0.2f, 0.85f, 1.0f, 0.45f});

        if (m_lifetimeTimer <= 0.0f) {
            if (!ItemWorld_RequestRespawn(self))
                ReleaseFromField(self);
        }
    }
}
