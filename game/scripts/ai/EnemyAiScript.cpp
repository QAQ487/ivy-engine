/*****************************************************
 * File: game/scripts/EnemyAiScript.cpp
 *****************************************************/
#include "EnemyAiScript.h"
#include "ItemSpawnManagerScript.h"
#include "ItemWorldApi.h"
#include "RoundManagerScript.h"
#include "MysteryBoxItem.h"
#include "PlayerScript.h"
#include "PlayerColor.h"
#include "game_score.h"
#include "GameMode.h"
#include "engine/physics/core/rigidbody.h"
#include "PointAreaScript.h"
#include "CenterPoleScript.h"
#include "../magnet/MagnetFieldSystem.h"
#include "../magnet/MagnetLinkVfx.h"
#include "../ui/PlayerNametag.h"
#include "engine/core/game_object.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/visual_component.h"
#include "engine/ecs/material_component.h"
#include "engine/ecs/collider_component.h"
#include "engine/physics/collider/collider.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "scene_manager.h"
#include "engine/ui/ui_text.h"
#include "engine/ui/ui_element.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <string>
#include <vector>

using namespace DirectX;

REGISTER_SCRIPT(EnemyAiScript)

static std::vector<EnemyAiScript*> g_activeEnemyScripts;

static constexpr int kPersonalitySlotCount = 3;
static constexpr int kPersonalityPoolSize = 6; // Balanced .. Cautious (skip Custom)
static int g_personalitySlots[kPersonalitySlotCount] = {};
static int g_personalityNextIndex = 0;
static bool g_personalitySlotsReady = false;

static int RandPersonalityIndex() {
    return std::rand() % kPersonalityPoolSize;
}

static bool IsValidPersonalitySet(const int* slots, int count) {
    if (count <= 0) return false;

    bool allSame = true;
    for (int i = 1; i < count; ++i) {
        if (slots[i] != slots[0]) {
            allSame = false;
            break;
        }
    }
    if (allSame) return false;

    int aggressors = 0;
    const int aggressor = static_cast<int>(EnemyPersonality::Aggressor);
    for (int i = 0; i < count; ++i) {
        if (slots[i] == aggressor)
            ++aggressors;
    }
    return aggressors <= 1;
}

static void RollPersonalitySlots() {
    do {
        for (int i = 0; i < kPersonalitySlotCount; ++i)
            g_personalitySlots[i] = RandPersonalityIndex();
    } while (!IsValidPersonalitySet(g_personalitySlots, kPersonalitySlotCount));
    g_personalityNextIndex = 0;
    g_personalitySlotsReady = true;
}

static int TakeNextPersonality() {
    if (!g_personalitySlotsReady)
        RollPersonalitySlots();
    if (g_personalityNextIndex >= kPersonalitySlotCount)
        return RandPersonalityIndex();
    return g_personalitySlots[g_personalityNextIndex++];
}

static XMFLOAT3 EnemyTintRgbForIndex(int playerIndex) {
    const XMFLOAT4 tint = (playerIndex >= 0 && playerIndex < PlayerColor_LobbySlotCount)
        ? PlayerColor_GetTint(PlayerColor_GetLobbySlot(playerIndex))
        : PlayerColor_GetTint(PlayerColor_GetEnemy());
    return {tint.x, tint.y, tint.z};
}

static void SyncEnemyTeamMaterial(engine::GameObject& self, int playerIndex) {
    if (auto* mat = self.GetComponent<MaterialComponent>())
        mat->color = EnemyTintRgbForIndex(playerIndex);
}

static void ClearEnemyEmissionGlow(engine::GameObject& self) {
    if (auto* vc = ScriptApi_FindVisualComponent(self))
        vc->emissionGlow = {0.0f, 0.0f, 0.0f, 0.0f};
}

namespace {

UIText* g_enemyAiDebugText = nullptr;

const char* ControlStateName(EnemyControlState state) {
    switch (state) {
        case EnemyControlState::Knockback: return "Knockback";
        default: return "Normal";
    }
}

const char* DriveModeName(EnemyDriveMode mode) {
    switch (mode) {
        case EnemyDriveMode::Cooldown: return "Cooldown";
        case EnemyDriveMode::ChaseBonus: return "ChaseBonus";
        case EnemyDriveMode::ChaseItem: return "ChaseItem";
        case EnemyDriveMode::ChasePlace: return "ChasePlace";
        case EnemyDriveMode::ChargeKnockback: return "ChargeKnockback";
        case EnemyDriveMode::MagAttach: return "MagPull";
        case EnemyDriveMode::MagOrbit: return "MagOrbit";
        case EnemyDriveMode::MagPush: return "MagPush";
        default: return "Wander";
    }
}

const char* ItemKindName(ItemWorldKind kind) {
    switch (kind) {
        case ItemWorldKind::ShockwavePole: return "ShockwavePole";
        case ItemWorldKind::SpeedReducePole: return "SpeedReducePole";
        case ItemWorldKind::Mine: return "Mine";
        case ItemWorldKind::ArrowSticker: return "ArrowSticker";
        default: return "none";
    }
}

void EnsureEnemyAiDebugText() {
    if (g_enemyAiDebugText) return;
    UICanvas* canvas = ScriptApi_SceneCanvasGetCanvas();
    if (!canvas) return;
    UIElement* element = ScriptApi_UICanvasGetElement(*canvas, "EnemyAiDebug");
    g_enemyAiDebugText = element ? dynamic_cast<UIText*>(element) : nullptr;
}

bool AnyEnemyWantsDebugUi() {
    for (EnemyAiScript* ai : g_activeEnemyScripts) {
        if (ai && ai->showAiDebug)
            return true;
    }
    return false;
}

} // namespace

static constexpr float kMoveSpeed              = 5.0f;
static constexpr float kMagnetForce            = 50.0f;
static constexpr float kMagnetMaxDist          = 47.0f;
static constexpr float kKnockbackDuration      = 0.45f;
static constexpr float kStunSpinSpeed          = 720.0f;
static constexpr float kItemSeekRadius         = 35.0f;
static constexpr float kItemPlaceMaxDistance   = 4.0f;
static constexpr float kItemPlaceCooldown      = 1.25f;
static constexpr float kChargeImpactForce      = 40.0f;
static constexpr float kChargeImpactLift       = 6.0f;
static constexpr float kChargeKnockbackDuration = 0.55f;
static constexpr float kChargeMinApproachSpeed = 5.0f;
static constexpr float kChargeCooldown         = 3.0f;
static constexpr float kChargeMaxDist          = 30.0f;
static constexpr float kChargeRecoilScale       = 0.3f;
static constexpr float kOrbitSpinPower         = 5.0f;
static constexpr float kOrbitMaxLaunchSpeed    = 50.0f;
static constexpr float kScoreLeadThreshold      = 5.0f;
static constexpr float kPoleAssistChance        = 0.28f;
static constexpr float kPathVariationMinDelay   = 0.45f;
static constexpr float kPathVariationMaxDelay   = 1.15f;
static constexpr float kArenaFootprintRefreshInterval = 0.25f;
static constexpr float kGroundCheckMinInterval  = 0.08f;
static constexpr float kGroundCheckMaxInterval  = 0.12f;
static constexpr float kDebugPanelRefreshInterval = 0.15f;
static constexpr float kMagObstacleScanInterval = 0.05f;

static constexpr float kBonusArriveSqr   = 1.8f * 1.8f;
static constexpr float kItemPickupScanRadius = 2.5f;
static constexpr float kMagComboCooldown = 2.5f;
static constexpr float kMagUtilityCooldown = 0.8f;
static constexpr float kMagFatigueWindow = 5.0f;
static constexpr float kMagFatigueCombatMultiplier = 0.4f;
static constexpr float kComboPullWindow = 1.5f;
static constexpr float kChargeAttackTimeout = 1.2f;
static constexpr float kChargeRushSpeedScale = 1.6f;
static constexpr float kTargetChargeDotThreshold = 4.0f;
static constexpr float kMagAttackFarDist = 18.0f;
static constexpr float kMagAttackMidDist = 8.0f;
static constexpr float kMagPullTouchMargin = 0.08f;
static constexpr float kMagPullTouchDistFallback = 2.05f;
static constexpr float kMinOrbitRadius   = 0.5f;
// Matches PlayerScript playerKnockbackImpulse used for charge self-recoil.
static constexpr float kChargeSelfRecoilBaseImpulse = 6.0f;
static constexpr float kChargeSelfRecoilLiftScale   = 0.35f;
static constexpr float kLocomotionSpeedSqThreshold = 0.25f;

static float ColliderContactReachXZ(const GameObject* obj) {
    if (!obj) return 0.5f;
    if (const Collider* col = obj->GetCollider()) {
        if (const auto* sphere = dynamic_cast<const SphereCollider*>(col))
            return sphere->GetRadius();
        if (const auto* box = dynamic_cast<const BoxCollider*>(col)) {
            const XMFLOAT3 size = ScriptApi_GetBoxColliderSize(*box);
            return 0.5f * (std::max)(size.x, size.z);
        }
        if (const auto* cap = dynamic_cast<const CapsuleCollider*>(col))
            return cap->GetRadius();
    }
    const XMFLOAT3 scale = obj->GetScale();
    return 0.5f * (std::max)(scale.x, scale.z);
}

static float DistSqXZBetweenObjects(const GameObject* a, const GameObject* b) {
    if (!a || !b) return 0.0f;
    XMFLOAT3 pa = a->GetPosition();
    XMFLOAT3 pb = b->GetPosition();
    if (const Collider* colA = a->GetCollider())
        pa = ScriptApi_GetColliderWorldCenter(*colA);
    if (const Collider* colB = b->GetCollider())
        pb = ScriptApi_GetColliderWorldCenter(*colB);
    const float dx = pa.x - pb.x;
    const float dz = pa.z - pb.z;
    return dx * dx + dz * dz;
}

struct EnemyCompetitor {
    GameObject* body = nullptr;
    int scoreIndex = -1;
    bool isPlayer = false;
};

struct EnemyCompetitorBuffer {
    EnemyCompetitor entries[kGameScoreMaxPlayers] = {};
    int count = 0;

    void Push(GameObject* body, int scoreIndex, bool isPlayer) {
        if (count >= kGameScoreMaxPlayers) return;
        entries[count++] = {body, scoreIndex, isPlayer};
    }
};

static bool IsValidCompetitorBody(GameObject* body) {
    return body && body->IsActive();
}

static bool IsPlayerBody(GameObject* body) {
    return body && body == PlayerScript_GetBody();
}

/** Soft teams: only filter when Team2v2 is active. Player is always team 0. */
static bool IsSameTeamAsSelf(const EnemyAiScript* self, int otherTeamId) {
    if (!self || GameMode_Get() != GameMode::Team2v2)
        return false;
    return self->GetTeamId() == otherTeamId;
}

static void AddCompetitors(EnemyCompetitorBuffer& out, const EnemyAiScript* self) {
    if (GameObject* player = PlayerScript_GetBody()) {
        if (!PlayerScript_IsRespawning() && IsValidCompetitorBody(player)
            && !IsSameTeamAsSelf(self, 0))
            out.Push(player, 0, true);
    }

    for (EnemyAiScript* ai : EnemyAi_GetActiveScripts()) {
        if (!ai || ai == self || EnemyAi_IsRespawning(ai)) continue;
        if (IsSameTeamAsSelf(self, ai->GetTeamId())) continue;
        GameObject* proxy = EnemyAi_GetPhysicsProxy(ai);
        if (!IsValidCompetitorBody(proxy)) continue;
        out.Push(proxy, ai->GetPlayerIndex(), false);
    }
}

/** Player fallback only when opposing team (or FFA). */
static GameObject* FallbackOpponentPlayer(const EnemyAiScript* self) {
    if (IsSameTeamAsSelf(self, 0))
        return nullptr;
    return PlayerScript_GetBody();
}

static bool IsMagPullBlockingObstacle(const GameObject* other,
                                      const GameObject* proxy,
                                      const GameObject* host) {
    if (!other || other == proxy) return false;
    if (host && other == host) return false;
    if (other->GetTag() == Tag::TAG_PLAYER) return false;
    if (other->GetTag() == Tag::TAG_ENEMY) return false;
    if (other->GetTag() == Tag::TAG_GROUND) return false;
    if (other->GetTag() == Tag::TAG_NONE) return false;

    const Collider* col = other->GetCollider();
    if (!col || col->IsTrigger()) return false;

    switch (other->GetTag()) {
        case Tag::TAG_WALL:
        case Tag::TAG_OBSTACLES:
            return true;
        default:
            return false;
    }
}

static constexpr float kWanderProbeDist  = 3.0f;
static constexpr float kArenaEdgeBlendStart = 0.85f;

class EnemyPhysicsProxy : public GameObject {
public:
    engine::Transform* m_syncTransform = nullptr;
    EnemyAiScript*     m_aiScript      = nullptr;

    ~EnemyPhysicsProxy() override {
        ScriptApi_DestroyCollider(*this);
    }

    void OnCollision(GameObject* other) override {
        if (!m_aiScript || !other || !other->IsActive()) return;
        if (!m_aiScript->IsMagPullActive()) return;

        XMFLOAT3 dir = {
            other->GetPosition().x - m_position.x,
            0.0f,
            other->GetPosition().z - m_position.z
        };
        const float lenSq = dir.x * dir.x + dir.z * dir.z;
        if (lenSq > 0.0001f) {
            const float inv = 1.0f / sqrtf(lenSq);
            dir.x *= inv;
            dir.z *= inv;
        } else {
            dir = {0.0f, 0.0f, 1.0f};
        }

        EnemyAi_OnProxyCollision(m_aiScript, dir, *other);
    }

    void LogicUpdate(double /*elapsed_time*/) override {}
};

static bool IsPositionInRadiusXZ(const XMFLOAT3& center, const XMFLOAT3& pos, float radiusSq) {
    const float dx = pos.x - center.x;
    const float dz = pos.z - center.z;
    return (dx * dx + dz * dz) <= radiusSq;
}

bool EnemyAi_IsAnyoneInRadius(const XMFLOAT3& center, float radius) {
    const float radiusSq = radius * radius;
    for (EnemyAiScript* ai : g_activeEnemyScripts) {
        if (!ai || ai->IsRespawning() || !ai->m_proxy || !ai->m_proxy->IsActive()) continue;
        if (IsPositionInRadiusXZ(center, ai->m_proxy->GetPosition(), radiusSq))
            return true;
    }
    return false;
}

void EnemyAi_ApplyStunInRadius(const XMFLOAT3& center,
                               float radius,
                               float stunDurationSeconds,
                               bool immobilize) {
    const float radiusSq = radius * radius;
    for (EnemyAiScript* ai : g_activeEnemyScripts) {
        if (!ai || ai->IsRespawning() || !ai->m_proxy || !ai->m_proxy->IsActive()) continue;
        if (!IsPositionInRadiusXZ(center, ai->m_proxy->GetPosition(), radiusSq)) continue;
        ai->BeginKnockback(stunDurationSeconds, immobilize);
    }
}

EnemyAiScript* EnemyAi_FindByPhysicsProxy(GameObject* proxy) {
    if (!proxy) return nullptr;
    for (EnemyAiScript* ai : g_activeEnemyScripts) {
        if (ai && ai->m_proxy == proxy)
            return ai;
    }
    return nullptr;
}

void EnemyAi_ApplyDirectionalImpulseToProxy(GameObject* proxy,
                                            const XMFLOAT3& direction,
                                            float impulse,
                                            float lift,
                                            float knockbackDurationSeconds) {
    EnemyAiScript* ai = EnemyAi_FindByPhysicsProxy(proxy);
    if (!ai || ai->IsRespawning() || !ai->m_proxy || !ai->m_proxy->IsActive())
        return;

    float dirLenSq = direction.x * direction.x + direction.z * direction.z;
    XMFLOAT3 dir = {0.0f, 0.0f, 1.0f};
    if (dirLenSq > 0.0001f) {
        const float invLen = 1.0f / sqrtf(dirLenSq);
        dir = {direction.x * invLen, 0.0f, direction.z * invLen};
    }

    const float useImpulse = (std::max)(0.0f, impulse);
    const float useLift = (std::max)(0.0f, lift);
    ai->BeginKnockback(knockbackDurationSeconds, false);
    Rigidbody& rb = ai->m_proxy->GetRigidbody();
    if (!rb.IsKinematic() && rb.GetMass() > 0.0f) {
        rb.AddForce(
            {dir.x * useImpulse, useLift, dir.z * useImpulse},
            ForceMode::VelocityChange);
    }
}

const std::vector<EnemyAiScript*>& EnemyAi_GetActiveScripts() {
    return g_activeEnemyScripts;
}

void EnemyAi_ResetSessionState() {
    if (g_enemyAiDebugText)
        ScriptApi_UIElementSetVisible(*g_enemyAiDebugText, false);
    g_enemyAiDebugText = nullptr;
    g_activeEnemyScripts.clear();
    g_personalitySlotsReady = false;
    g_personalityNextIndex = 0;
}

bool EnemyAi_IsRespawning(const EnemyAiScript* ai) {
    return ai && ai->IsRespawning();
}

bool EnemyAi_IsBodyRespawning(const GameObject* body) {
    if (!body) return false;
    for (const EnemyAiScript* ai : g_activeEnemyScripts) {
        if (EnemyAi_IsRespawning(ai) && EnemyAi_GetPhysicsProxy(ai) == body)
            return true;
    }
    return false;
}

GameObject* EnemyAi_GetPhysicsProxy(const EnemyAiScript* ai) {
    return ai ? ai->GetPhysicsProxy() : nullptr;
}

GameObject* EnemyAi_GetHostObject(EnemyAiScript* ai) {
    return ai ? ai->GetHostObject() : nullptr;
}

void EnemyAi_BeginRespawnAt(EnemyAiScript* ai, const DirectX::XMFLOAT3& holdPosition) {
    if (ai) ai->BeginRespawnAt(holdPosition);
}

void EnemyAi_TickRespawnHold(EnemyAiScript* ai) {
    if (ai) ai->TickRespawnHold();
}

void EnemyAi_EndRespawn(EnemyAiScript* ai) {
    if (ai) ai->EndRespawn();
}

bool EnemyAi_TryGetSpawnPosition(const EnemyAiScript* ai, DirectX::XMFLOAT3& outPosition) {
    if (!ai) return false;
    outPosition = ai->GetSpawnPosition();
    return true;
}

void EnemyAi_OnProxyCollision(EnemyAiScript* ai,
                              const XMFLOAT3& dirFromAiToOther,
                              GameObject& otherBody) {
    if (!ai || ai->IsRespawning() || !ai->m_proxy || !otherBody.IsActive()) return;

    GameObject* player = PlayerScript_GetBody();
    if (ai->IsMagPullActive() &&
        ((player && &otherBody == player) || (&otherBody == ai->m_magnetTarget))) {
        ai->TryFinishMagPullPlayerHit(&otherBody);
        return;
    }

    if (ai->IsMagPullActive()) {
        if (!IsMagPullBlockingObstacle(&otherBody, ai->m_proxy, ai->m_hostObject))
            return;
        ai->EndMagPull(true);
        return;
    }

    ai->BeginKnockback(-1.0f, false);
    ai->m_proxy->GetRigidbody().AddForce(
        {-dirFromAiToOther.x * 4.0f, 0.0f, -dirFromAiToOther.z * 4.0f},
        ForceMode::Impulse);
    otherBody.GetRigidbody().AddForce(
        {dirFromAiToOther.x * 4.0f, 0.0f, dirFromAiToOther.z * 4.0f},
        ForceMode::Impulse);
}

void EnemyAi_ApplyShockwaveImpulse(const XMFLOAT3& center,
                                   float radius,
                                   float baseImpulse,
                                   float lift,
                                   float knockbackDurationSeconds,
                                   GameObject* placerBody,
                                   float placerImpulseScale) {
    const float radiusSq = radius * radius;
    const float placerScale = (std::max)(0.0f, placerImpulseScale);
    for (EnemyAiScript* ai : g_activeEnemyScripts) {
        if (!ai || ai->IsRespawning() || !ai->m_proxy || !ai->m_proxy->IsActive()) continue;

        XMFLOAT3 delta = {
            ai->m_proxy->GetPosition().x - center.x,
            0.0f,
            ai->m_proxy->GetPosition().z - center.z
        };
        const float distSq = delta.x * delta.x + delta.z * delta.z;
        if (distSq > radiusSq) continue;

        float dist = sqrtf(distSq);
        XMFLOAT3 dir = {1.0f, 0.0f, 0.0f};
        if (dist > 0.001f) {
            const float invDist = 1.0f / dist;
            dir = {delta.x * invDist, 0.0f, delta.z * invDist};
        }

        float falloff = 1.0f - (dist / radius);
        falloff = (std::max)(0.15f, falloff);
        const float impulseScale = (placerBody && ai->m_proxy == placerBody) ? placerScale : 1.0f;
        const float impulse = baseImpulse * falloff * impulseScale;
        const float scaledLift = lift * falloff * impulseScale;

        ai->BeginKnockback(knockbackDurationSeconds, false);
        ai->m_proxy->GetRigidbody().AddForce(
            {dir.x * impulse, scaledLift, dir.z * impulse},
            ForceMode::VelocityChange);
    }
}

// ---- Personality presets ------------------------------------------------------

static EnemyAiTuning MakeTuning(float aggro,
                              float bonusWeight,
                              float itemWeight,
                              float combatWeight,
                              float utilityMagnetWeight,
                              float intentNoise,
                              float comboMultiplier,
                              float hesitationChance,
                              float wanderBaseDelay,
                              float wanderBlunderChance,
                              float itemChaseChance,
                              float playerAttackChance,
                              float chargeChance,
                              float reactionDelayMin,
                              float reactionDelayMax,
                              float magTravelMinDist,
                              float magUtilityEnergyMin,
                              float comboChainChance,
                              float midChaseTravelChance,
                              float utilityCooldownScale) {
    EnemyAiTuning t;
    t.aggro = aggro;
    t.bonusWeight = bonusWeight;
    t.itemWeight = itemWeight;
    t.combatWeight = combatWeight;
    t.utilityMagnetWeight = utilityMagnetWeight;
    t.intentNoise = intentNoise;
    t.comboMultiplier = comboMultiplier;
    t.hesitationChance = hesitationChance;
    t.wanderBaseDelay = wanderBaseDelay;
    t.wanderBlunderChance = wanderBlunderChance;
    t.itemChaseChance = itemChaseChance;
    t.playerAttackChance = playerAttackChance;
    t.chargeChance = chargeChance;
    t.reactionDelayMin = reactionDelayMin;
    t.reactionDelayMax = reactionDelayMax;
    t.magTravelMinDist = magTravelMinDist;
    t.magUtilityEnergyMin = magUtilityEnergyMin;
    t.comboChainChance = comboChainChance;
    t.midChaseTravelChance = midChaseTravelChance;
    t.utilityCooldownScale = utilityCooldownScale;
    return t;
}

const char* EnemyAiScript::PersonalityName(int personalityValue) {
    switch (static_cast<EnemyPersonality>(personalityValue)) {
        case EnemyPersonality::PointHunter: return "PointHunter";
        case EnemyPersonality::Aggressor: return "Aggressor";
        case EnemyPersonality::Collector: return "Collector";
        case EnemyPersonality::Trickster: return "Trickster";
        case EnemyPersonality::Cautious: return "Cautious";
        case EnemyPersonality::ExtremeAggressor: return "ExtremeAggressor";
        case EnemyPersonality::Custom: return "Custom";
        default: return "Balanced";
    }
}

EnemyAiTuning EnemyAiScript::BuildTuningFromAdvancedFields() const {
    return MakeTuning(
        1.0f,
        0.42f,
        0.42f,
        0.55f,
        1.0f,
        intentNoise,
        1.0f,
        0.10f,
        0.28f,
        0.14f,
        itemChaseChance,
        playerAttackChance,
        chargeChance,
        reactionDelayMin,
        reactionDelayMax,
        magTravelMinDist,
        magUtilityEnergyMin,
        comboChainChance,
        midChaseTravelChance,
        utilityCooldownScale);
}

void EnemyAiScript::OverlayAdvancedFieldsOntoTuning() {
    EnemyAiTuning advanced = BuildTuningFromAdvancedFields();
    m_tuning.itemChaseChance = advanced.itemChaseChance;
    m_tuning.playerAttackChance = advanced.playerAttackChance;
    m_tuning.chargeChance = advanced.chargeChance;
    m_tuning.intentNoise = advanced.intentNoise;
    m_tuning.reactionDelayMin = advanced.reactionDelayMin;
    m_tuning.reactionDelayMax = advanced.reactionDelayMax;
    m_tuning.magTravelMinDist = advanced.magTravelMinDist;
    m_tuning.magUtilityEnergyMin = advanced.magUtilityEnergyMin;
    m_tuning.comboChainChance = advanced.comboChainChance;
    m_tuning.midChaseTravelChance = advanced.midChaseTravelChance;
    m_tuning.utilityCooldownScale = advanced.utilityCooldownScale;
}

void EnemyAiScript::ApplyPersonalityPreset(EnemyPersonality preset) {
    switch (preset) {
        case EnemyPersonality::PointHunter:
            m_tuning = MakeTuning(
                1.10f, 0.85f, 0.30f, 0.45f, 0.60f,
                0.10f, 0.85f, 0.08f,
                0.24f, 0.10f,
                0.35f, 0.90f, 0.32f,
                0.15f, 0.35f,
                20.0f, 0.40f, 0.35f, 0.25f, 2.0f);
            break;
        case EnemyPersonality::Aggressor:
            m_tuning = MakeTuning(
                1.25f, 0.25f, 0.22f, 0.80f, 0.50f,
                0.08f, 1.10f, 0.05f,
                0.16f, 0.04f,
                0.28f, 1.15f, 0.55f,
                0.12f, 0.28f,
                20.0f, 0.35f, 0.40f, 0.20f, 2.0f);
            break;
        case EnemyPersonality::ExtremeAggressor:
            // All-in hunter: ignore loot/points, instant reactions, near-constant charge.
            m_tuning = MakeTuning(
                1.85f, 0.08f, 0.05f, 1.25f, 0.25f,
                0.02f, 1.40f, 0.0f,
                0.06f, 0.0f,
                0.08f, 1.70f, 0.95f,
                0.02f, 0.08f,
                14.0f, 0.20f, 0.65f, 0.10f, 1.2f);
            break;
        case EnemyPersonality::Collector:
            m_tuning = MakeTuning(
                1.15f, 0.30f, 0.75f, 0.40f, 0.70f,
                0.11f, 0.90f, 0.08f,
                0.20f, 0.08f,
                0.62f, 0.85f, 0.30f,
                0.16f, 0.38f,
                19.0f, 0.38f, 0.38f, 0.28f, 2.0f);
            break;
        case EnemyPersonality::Trickster:
            m_tuning = MakeTuning(
                1.05f, 0.35f, 0.38f, 0.62f, 1.10f,
                0.10f, 1.30f, 0.07f,
                0.22f, 0.10f,
                0.40f, 1.0f, 0.40f,
                0.14f, 0.34f,
                16.0f, 0.32f, 0.55f, 0.35f, 1.6f);
            break;
        case EnemyPersonality::Cautious:
            m_tuning = MakeTuning(
                0.72f, 0.58f, 0.28f, 0.42f, 0.75f,
                0.20f, 0.55f, 0.20f,
                0.38f, 0.24f,
                0.28f, 0.75f, 0.25f,
                0.22f, 0.48f,
                22.0f, 0.42f, 0.30f, 0.22f, 2.2f);
            break;
        case EnemyPersonality::Balanced:
        default:
            m_tuning = MakeTuning(
                1.0f, 0.42f, 0.42f, 0.55f, 1.0f,
                0.12f, 1.0f, 0.10f,
                0.28f, 0.14f,
                0.42f, 1.0f, 0.38f,
                0.15f, 0.40f,
                18.0f, 0.35f, 0.45f, 0.30f, 2.0f);
            break;
    }
}

void EnemyAiScript::ResolveTuning() {
    const EnemyPersonality preset = static_cast<EnemyPersonality>(personality);
    if (preset == EnemyPersonality::Custom)
        m_tuning = BuildTuningFromAdvancedFields();
    else
        ApplyPersonalityPreset(preset);

    if (showAdvancedTuning && preset != EnemyPersonality::Custom)
        OverlayAdvancedFieldsOntoTuning();
}

nlohmann::json EnemyAiScript::Serialize() const {
    nlohmann::json j;
    ENEMY_AI_PUBLIC_FIELDS(SCRIPT_FIELD_TO_JSON)
    if (showAdvancedTuning)
        ENEMY_AI_ADVANCED_FIELDS(SCRIPT_FIELD_TO_JSON)
    return j;
}

void EnemyAiScript::Deserialize(const nlohmann::json& j) {
    ENEMY_AI_PUBLIC_FIELDS(SCRIPT_FIELD_FROM_JSON)
    if (j.contains("aiProfile") && !j.contains("personality")) {
        const int oldProfile = j["aiProfile"].get<int>();
        switch (oldProfile) {
            case 1: personality = static_cast<int>(EnemyPersonality::Cautious); break;
            case 2: personality = static_cast<int>(EnemyPersonality::Collector); break;
            case 3: personality = static_cast<int>(EnemyPersonality::Balanced); break;
            default: personality = static_cast<int>(EnemyPersonality::Balanced); break;
        }
    }
    ENEMY_AI_ADVANCED_FIELDS(SCRIPT_FIELD_FROM_JSON)
    ResolveTuning();
}

static float RandRange(float lo, float hi) {
    return lo + (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * (hi - lo);
}

float EnemyAiScript::ProfileAggro() const {
    return m_tuning.aggro;
}

float EnemyAiScript::Rand01() const {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

int EnemyAiScript::GetPlayerIndex() const {
    if (playerIndex < 1 || playerIndex >= kGameScoreMaxPlayers)
        return 1;
    return playerIndex;
}

bool EnemyAiScript::IsMagPullActive() const {
    return m_mode == EnemyDriveMode::MagAttach;
}

bool EnemyAiScript::IsMagneticModeActive() const {
    return m_mode == EnemyDriveMode::MagAttach ||
           m_mode == EnemyDriveMode::MagPush ||
           m_mode == EnemyDriveMode::MagOrbit;
}

GameObject* EnemyAiScript::SelectAttackTarget(int* outScoreIndex) const {
    if (outScoreIndex) *outScoreIndex = -1;
    if (!m_proxy) return nullptr;

    EnemyCompetitorBuffer competitors;
    AddCompetitors(competitors, this);
    if (competitors.count == 0) return nullptr;

    const int myScoreIndex = GetPlayerIndex();
    const int myScore = GameScore_Get(myScoreIndex);
    GameObject* bestBody = nullptr;
    int bestScoreIndex = -1;
    float bestValue = -100000.0f;

    for (int i = 0; i < competitors.count; ++i) {
        const EnemyCompetitor& c = competitors.entries[i];
        if (!c.body || c.scoreIndex == myScoreIndex) continue;

        const int score = GameScore_Get(c.scoreIndex);
        const float distSq = DistSqXZBetweenObjects(m_proxy, c.body);
        const float dist = sqrtf((std::max)(0.001f, distSq));
        const float scorePressure = static_cast<float>(score - myScore) * 2.0f;
        const float leaderBonus = score >= myScore ? 12.0f : 0.0f;
        const float playerBonus = c.isPlayer ? 2.0f : 0.0f;
        const float value = scorePressure + leaderBonus + playerBonus - dist * 0.35f;
        if (value > bestValue) {
            bestValue = value;
            bestBody = c.body;
            bestScoreIndex = c.scoreIndex;
        }
    }

    if (outScoreIndex) *outScoreIndex = bestScoreIndex;
    return bestBody;
}

GameObject* EnemyAiScript::SelectPoleAssistTarget(const XMFLOAT3& goal) const {
    if (!m_proxy) return nullptr;

    XMFLOAT3 dir = {
        goal.x - m_proxy->GetPosition().x,
        0.0f,
        goal.z - m_proxy->GetPosition().z
    };
    const float lenSq = dir.x * dir.x + dir.z * dir.z;
    if (lenSq < 0.0001f) return nullptr;

    const float invLen = 1.0f / sqrtf(lenSq);
    dir.x *= invLen;
    dir.z *= invLen;

    const int index = CenterPole_FindBestTargetInCone(m_proxy->GetPosition(), dir, kMagnetMaxDist);
    GameObject* node = CenterPole_GetRegistryNode(index);
    if (!node || node == m_proxy || node->GetTag() == Tag::TAG_PLAYER || node->GetTag() == Tag::TAG_ENEMY)
        return nullptr;
    return node;
}

const char* EnemyAiScript::IntentName(EnemyIntent intent) {
    switch (intent) {
        case EnemyIntent::ChaseItem: return "ChaseItem";
        case EnemyIntent::ChaseBonus: return "ChaseBonus";
        case EnemyIntent::ChasePlace: return "ChasePlace";
        case EnemyIntent::MagTravel: return "MagTravel";
        case EnemyIntent::MagRetreat: return "MagRetreat";
        case EnemyIntent::MagAttackPull: return "MagAttackPull";
        case EnemyIntent::MagAttackOrbit: return "MagAttackOrbit";
        case EnemyIntent::ChargeAttack: return "ChargeAttack";
        default: return "Wander";
    }
}

const char* EnemyAiScript::MagPurposeName(EnemyMagPurpose purpose) {
    switch (purpose) {
        case EnemyMagPurpose::Utility: return "Utility";
        case EnemyMagPurpose::Combat: return "Combat";
        default: return "None";
    }
}

bool EnemyAiScript::IsTargetChargingToward(const GameObject* target) const {
    if (!m_proxy || !target) return false;

    XMFLOAT3 toAi = {
        m_proxy->GetPosition().x - target->GetPosition().x,
        0.0f,
        m_proxy->GetPosition().z - target->GetPosition().z
    };
    const float lenSq = toAi.x * toAi.x + toAi.z * toAi.z;
    if (lenSq < 0.0001f) return false;
    const float inv = 1.0f / sqrtf(lenSq);
    toAi.x *= inv;
    toAi.z *= inv;

    XMFLOAT3 vel = target->GetRigidbody().GetVelocity();
    vel.y = 0.0f;
    return (vel.x * toAi.x + vel.z * toAi.z) >= kTargetChargeDotThreshold;
}

void EnemyAiScript::UpdateArenaFootprint() {
    m_hasArenaFootprint = false;
    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (!scene) return;

    float bestArea = -1.0f;
    for (auto* obj : scene->GetObjects()) {
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
            m_arenaCenter = center;
            m_arenaSize = size;
            m_hasArenaFootprint = true;
        }
    }

    if (m_hasArenaFootprint) {
        const float floorRadius = m_arenaSize.x * 0.5f;
        m_arenaSafeRadius = (std::max)(0.5f, floorRadius - kArenaEdgePadding);
    }
}

void EnemyAiScript::TickArenaFootprint(float dt) {
    m_arenaFootprintRefreshTimer -= dt;
    if (m_hasArenaFootprint && m_arenaFootprintRefreshTimer > 0.0f)
        return;

    UpdateArenaFootprint();
    m_arenaFootprintRefreshTimer = kArenaFootprintRefreshInterval;
}

void EnemyAiScript::SteerMoveTowardSafeXZ(float& moveX, float& moveZ) const {
    if (!m_hasArenaFootprint || !m_proxy) return;
    if (moveX == 0.0f && moveZ == 0.0f) return;

    const XMFLOAT3 pos = m_proxy->GetPosition();
    float dx = m_arenaCenter.x - pos.x;
    float dz = m_arenaCenter.z - pos.z;
    const float distSq = dx * dx + dz * dz;
    if (distSq < 0.0001f) return;

    const float dist = sqrtf(distSq);
    const float edgeStart = m_arenaSafeRadius * kArenaEdgeBlendStart;
    if (dist <= edgeStart) return;

    const float t = (std::min)(1.0f, (dist - edgeStart) / (std::max)(0.01f, m_arenaSafeRadius - edgeStart));
    const float invDist = 1.0f / dist;
    const float toCenterX = dx * invDist;
    const float toCenterZ = dz * invDist;

    moveX = moveX * (1.0f - t) + toCenterX * t;
    moveZ = moveZ * (1.0f - t) + toCenterZ * t;

    const float len = sqrtf(moveX * moveX + moveZ * moveZ);
    if (len > 0.01f) {
        moveX /= len;
        moveZ /= len;
    }
}

bool EnemyAiScript::IsNearArenaEdge() const {
    if (!m_hasArenaFootprint || !m_proxy) return false;
    const XMFLOAT3 pos = m_proxy->GetPosition();
    const float dx = pos.x - m_arenaCenter.x;
    const float dz = pos.z - m_arenaCenter.z;
    const float dist = sqrtf(dx * dx + dz * dz);
    return dist >= m_arenaSafeRadius * kArenaEdgeBlendStart;
}

bool EnemyAiScript::IsDirectionTowardArenaCenter(float dirX, float dirZ) const {
    if (!m_hasArenaFootprint || !m_proxy) return true;
    const XMFLOAT3 pos = m_proxy->GetPosition();
    const float probeX = pos.x + dirX * kWanderProbeDist;
    const float probeZ = pos.z + dirZ * kWanderProbeDist;
    const float pdx = probeX - m_arenaCenter.x;
    const float pdz = probeZ - m_arenaCenter.z;
    return (pdx * pdx + pdz * pdz) <= m_arenaSafeRadius * m_arenaSafeRadius;
}

bool EnemyAiScript::IsAimedAtPlayerForCharge(GameObject* player) const {
    if (!player || !m_proxy) return false;

    XMFLOAT3 toPlayer = {
        player->GetPosition().x - m_proxy->GetPosition().x,
        0.0f,
        player->GetPosition().z - m_proxy->GetPosition().z
    };
    const float lenSq = toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z;
    if (lenSq < 0.0001f) return false;
    const float inv = 1.0f / sqrtf(lenSq);
    toPlayer.x *= inv;
    toPlayer.z *= inv;

    const int aimIdx = CenterPole_FindBestTargetInCone(
        m_proxy->GetPosition(), toPlayer, kChargeMaxDist);
    if (aimIdx < 0) return false;
    return CenterPole_GetRegistryNode(aimIdx) == player;
}

bool EnemyAiScript::MeetsChargeApproachSpeed(GameObject* player,
                                             const XMFLOAT3& dirToPlayer) const {
    (void)player;
    if (!m_proxy || kChargeMinApproachSpeed <= 0.0f) return true;

    XMFLOAT3 vel = m_proxy->GetRigidbody().GetVelocity();
    vel.y = 0.0f;
    return (vel.x * dirToPlayer.x + vel.z * dirToPlayer.z) >= kChargeMinApproachSpeed;
}

// ---- Lifecycle --------------------------------------------------------------

void EnemyAiScript::OnCreate(engine::GameObject& self) {
    if (GameMode_Get() == GameMode::ExtremeBattle)
        personality = static_cast<int>(EnemyPersonality::ExtremeAggressor);
    else if (GameMode_IsTraining())
        personality = static_cast<int>(EnemyPersonality::Balanced);
    else
        personality = TakeNextPersonality();
    ResolveTuning();
    m_hostObject = &self;
    m_proxy = new EnemyPhysicsProxy();
    m_proxy->m_aiScript = this;
    m_proxy->m_position = { -5.0f, 1.0f, 5.0f };
    m_proxy->m_scale    = { 1.0f, 1.0f, 1.0f };

    if (ColliderComponent* cc = ScriptApi_FindColliderComponent(self)) {
        GameObject* savedOwner = cc->owner;
        cc->owner = m_proxy;
        ScriptApi_ApplyColliderComponent(*cc);
        cc->owner = savedOwner;
        m_proxy->m_colliderOwnedByHost = true;
        m_proxy->m_debugCollider = cc->showDebugCollider;
    } else {
        ScriptApi_CreateBoxCollider(*m_proxy, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f});
        m_proxy->m_debugCollider = true;
    }
    m_proxy->m_rigidbody.SetIsKinematic(false);
    m_proxy->m_rigidbody.SetUseGravity(true);
    m_proxy->m_rigidbody.SetDrag(1.0f);
    m_proxy->m_rigidbody.SetMass(1.0f);
    m_proxy->SetTag(self.GetTag());

    ScriptApi_DestroyCollider(self);
    self.m_debugCollider = false;

    if (SceneBase* scene = ScriptApi_GetCurrentScene())
        scene->AddObject(m_proxy);

    // Prefer authored SceneTransform from scene JSON.
    XMFLOAT3 spawnPos = self.GetPosition();
    engine::Transform* transform = ScriptApi_FindTransform(self);
    if (transform) {
        spawnPos = transform->localPosition;
        m_yaw = transform->localRotation.y;
    }

    m_proxy->SetPosition(spawnPos);
    m_proxy->m_syncTransform = transform;

    // Match modes rematerialize onto fixed ring. Training keeps JSON pose.
    if (!GameMode_IsTraining()) {
        RoundManager_EnsureMinSpawnRadius(spawnPos);
        m_proxy->SetPosition(spawnPos);
    }

    // Post-script ResolveCollisions sweeps previous→current. New proxies default
    // previous to (0,0,0); a teleport from origin through CenterPole sticks the body on the pole.
    m_proxy->m_previousPosition = m_proxy->m_position;

    if (transform)
        transform->localPosition = m_proxy->GetPosition();
    self.SetPosition(m_proxy->GetPosition());

    m_spawnPosition = m_proxy->m_position;
    m_isRespawning  = false;
    m_controlState  = EnemyControlState::Normal;
    m_knockbackTimer = 0.0f;
    m_mode          = EnemyDriveMode::Wander;
    m_modeTimer     = 0.0f;
    m_rethinkTimer  = RandRange(0.35f, 0.95f) / ProfileAggro();
    m_magCombatCooldown = 0.0f;
    m_magUtilityCooldown = 0.0f;
    m_reactionTimer = 0.0f;
    m_intentCommitTimer = 0.0f;
    m_currentIntent = EnemyIntent::Wander;
    m_pendingIntent = EnemyIntent::Wander;
    m_magPurpose = EnemyMagPurpose::None;
    m_magFatigueTimer = 0.0f;
    m_recentCombatMagUses = 0;
    m_comboStep = 0;
    m_comboPullTimer = 0.0f;
    m_comboTarget = nullptr;
    m_postOrbitResumeMode = EnemyDriveMode::Wander;
    m_postOrbitResumeActive = false;
    m_justExitedKnockback = false;
    for (float& score : m_debugIntentScores)
        score = 0.0f;
    m_magnetTarget = nullptr;
    m_magnetTargetScoreIndex = -1;
    m_itemChaseTarget = nullptr;
    m_heldItem = nullptr;
    m_heldItemKind = ItemWorldKind::Unknown;
    m_itemPlaceCooldown = 0.0f;
    m_chargeCooldownRemaining = 0.0f;
    m_magneticEnergy = kMagneticEnergyMax;
    m_magneticOverheated = false;
    m_hasArenaFootprint = false;
    m_arenaFootprintRefreshTimer = 0.0f;
    m_groundCheckTimer = RandRange(kGroundCheckMinInterval, kGroundCheckMaxInterval);
    m_debugPanelTimer = RandRange(0.0f, kDebugPanelRefreshInterval);
    m_magObstacleScanTimer = RandRange(0.0f, kMagObstacleScanInterval);
    m_wanderDirX = 1.0f;
    m_wanderDirZ = 0.0f;
    m_pathVariationTimer = RandRange(kPathVariationMinDelay, kPathVariationMaxDelay);
    m_pathSide = Rand01() < 0.5f ? -1.0f : 1.0f;
    m_pathStrength = RandRange(0.15f, 0.4f);
    m_wanderTimer = m_tuning.wanderBaseDelay * (0.5f + 0.5f * Rand01());
    m_usedMidChaseTravel = false;

    CenterPole_AddDynamicTarget(m_proxy->m_position, m_proxy, &m_registryIndex);
    g_activeEnemyScripts.push_back(this);
    SetupVisualAnimations(self);
    BindMagneticEnergyUI();
}

void EnemyAiScript::SetupVisualAnimations(engine::GameObject& self) {
    VisualComponent* vc = ScriptApi_FindVisualComponent(self);
    if (!vc) return;

    vc->playAnimation = true;
    if (vc->animationPaths.find(runClip) == vc->animationPaths.end())
        vc->animationPaths[runClip] = "rom/model/player/run.fbx";
    if (vc->animationPaths.find(idleClip) == vc->animationPaths.end())
        vc->animationPaths[idleClip] = "rom/model/player/idle.fbx";
    vc->emissionGlow = {0.0f, 0.0f, 0.0f, 0.0f};
    SyncEnemyTeamMaterial(self, GetPlayerIndex());
}

bool EnemyAiScript::IsEnemyLocomoting() const {
    if (m_isRespawning || m_stunImmobilize)
        return false;

    if (!m_proxy) return false;
    XMFLOAT3 vel = m_proxy->GetRigidbody().GetVelocity();
    const float speedSq = vel.x * vel.x + vel.z * vel.z;
    if (speedSq > kLocomotionSpeedSqThreshold)
        return true;

    if (fabsf(m_moveX) > 0.01f || fabsf(m_moveZ) > 0.01f)
        return true;

    return false;
}

void EnemyAiScript::SyncEnemyAnimation(engine::GameObject& self) {
    VisualComponent* vc = ScriptApi_FindVisualComponent(self);
    if (!vc || !vc->playAnimation) return;

    const std::string targetClip = IsEnemyLocomoting() ? runClip : idleClip;
    ScriptApi_Visual_PlayClip(vc, targetClip.c_str());
}

void EnemyAiScript::BindMagneticEnergyUI() {
    UICanvas* canvas = ScriptApi_SceneCanvasGetCanvas();
    if (!canvas)
        return;

    char spriteName[32];
    snprintf(spriteName, sizeof(spriteName), "HudP%dEnergyBar", GetPlayerIndex() + 1);
    m_magneticEnergyBarSprite =
        ScriptApi_UICanvasGetSprite(*canvas, spriteName);
    if (!m_magneticEnergyBarSprite)
        return;

    m_magneticEnergyBarSprite->SetVisible(true);
    SyncMagneticEnergyUI();
}

void EnemyAiScript::EnterMagneticOverheat() {
    m_magneticOverheated = true;
    m_magneticEnergy = 0.0f;
    if (IsMagPullActive())
        EndMagPull(true);
}

MagnetHelixPath::MagnetHelixSettings EnemyAiScript::BuildMagnetHelixSettings() const {
    MagnetHelixPath::MagnetHelixSettings settings;
    settings.ampMin = helixAmpMin;
    settings.ampMax = helixAmpMax;
    settings.turnsMin = helixTurnsMin;
    settings.turnsMax = helixTurnsMax;
    settings.rotateSpeed = helixRotateSpeed;
    settings.travelTime = helixTravelTime;
    settings.particleSize = helixParticleSize;
    return settings;
}

void EnemyAiScript::SyncMagnetLinkVfx(GameObject* target) {
    if (!m_proxy || !target || !target->IsActive() || !CanUseMagnetic())
        return;
    if (!IsMagneticModeActive())
        return;

    int linkType = 1;
    if (m_mode == EnemyDriveMode::MagPush)
        linkType = 2;
    else if (m_mode == EnemyDriveMode::MagOrbit)
        linkType = 3;

    MagnetFieldSystem::Instance().ShowMagnetLink(*m_proxy, *target, linkType);
}

bool EnemyAiScript::CanUseMagnetic() const {
    return !m_magneticOverheated;
}

bool EnemyAiScript::IsMagneticEnergyDraining(GameObject* target) const {
    if (!CanUseMagnetic() || !IsMagneticModeActive())
        return false;
    if (m_mode == EnemyDriveMode::MagAttach && target && IsTouchingTarget(target))
        return false;
    if (m_magPurpose == EnemyMagPurpose::Utility)
        return true;
    return true;
}

void EnemyAiScript::TickMagneticEnergy(float dt) {
    if (dt <= 0.0f)
        return;

    const float maxEnergy = kMagneticEnergyMax;
    const float unlockEnergy = maxEnergy * kMagneticEnergyOverheatUnlockRatio;
    const float utilityDrainScale = 0.5f;

    if (m_magneticOverheated) {
        m_magneticEnergy += kMagneticEnergyRegenPerSec * dt;
        if (m_magneticEnergy >= unlockEnergy)
            m_magneticOverheated = false;
        m_magneticEnergy = (std::min)(maxEnergy, m_magneticEnergy);
        return;
    }

    GameObject* target = m_magnetTarget ? m_magnetTarget : PlayerScript_GetBody();
    if (IsMagneticEnergyDraining(target)) {
        float drainRate = kMagneticEnergyDrainPerSec;
        if (m_magPurpose == EnemyMagPurpose::Utility)
            drainRate *= utilityDrainScale;
        m_magneticEnergy -= drainRate * dt;
        if (m_magneticEnergy <= 0.0f)
            EnterMagneticOverheat();
        else
            m_magneticEnergy = (std::max)(0.0f, m_magneticEnergy);
        return;
    }

    m_magneticEnergy += kMagneticEnergyRegenPerSec * dt;
    m_magneticEnergy = (std::min)(maxEnergy, m_magneticEnergy);
}

void EnemyAiScript::SyncMagneticEnergyUI() const {
    if (!m_magneticEnergyBarSprite)
        return;

    const float fill = (std::max)(0.0f,
        (std::min)(1.0f, m_magneticEnergy / (std::max)(0.01f, kMagneticEnergyMax)));
    m_magneticEnergyBarSprite->SetVisible(true);
    ScriptApi_UISpriteSetRadial360Fill(*m_magneticEnergyBarSprite, fill, false);
}

GameObject* EnemyAiScript::GetRuntimePhysicsProxy(const GameObject& /*self*/) const {
    return m_proxy;
}

void EnemyAiScript::OnDestroy(engine::GameObject& self) {
    (void)self;
    if (m_isRespawning)
        m_isRespawning = false;
    g_activeEnemyScripts.erase(
        std::remove(g_activeEnemyScripts.begin(), g_activeEnemyScripts.end(), this),
        g_activeEnemyScripts.end());
    if (g_activeEnemyScripts.empty() && g_enemyAiDebugText)
        ScriptApi_UIElementSetVisible(*g_enemyAiDebugText, false);
    m_magneticEnergyBarSprite = nullptr;
    CenterPole_RemoveDynamicTarget(m_registryIndex);
    if (m_proxy) {
        if (SceneBase* scene = ScriptApi_GetCurrentScene())
            scene->RemoveObject(m_proxy);
        delete m_proxy;
        m_proxy = nullptr;
    }
}

// ---- Control state -----------------------------------------------------------

GameObject* EnemyAiScript::GetPhysicsProxy() const {
    return m_proxy;
}

void EnemyAiScript::SyncRespawnVisual() {
    if (m_hostObject) {
        m_hostObject->SetActive(true);
        if (engine::Transform* t = ScriptApi_FindTransform(*m_hostObject)) {
            t->localPosition = m_respawnHoldPosition;
            t->localRotation.y = m_yaw;
        }
    }

    if (m_registryIndex >= 0)
        CenterPole_UpdateDynamicPosition(m_registryIndex, m_respawnHoldPosition);
}

void EnemyAiScript::SetHeldItem(GameObject* item, ItemWorldKind kind) {
    m_heldItem = item;
    m_heldItemKind = kind;
}

void EnemyAiScript::ClearHeldItem() {
    m_heldItem = nullptr;
    m_heldItemKind = ItemWorldKind::Unknown;
}

void EnemyAiScript::BeginRespawnAt(const XMFLOAT3& holdPosition) {
    if (m_isRespawning) return;

    m_itemChaseTarget = nullptr;
    ExitMagneticModes();
    m_controlState = EnemyControlState::Normal;
    m_knockbackTimer = 0.0f;
    m_stunImmobilize = false;
    m_stunSpinAngle = 0.0f;

    m_respawnHoldPosition = holdPosition;

    if (m_proxy) {
        m_respawnGravityEnabled = m_proxy->GetRigidbody().UseGravity();
        m_proxy->SetActive(true);
        m_proxy->GetRigidbody().SetUseGravity(false);
        m_proxy->GetRigidbody().SetVelocity({0.0f, 0.0f, 0.0f});
        m_proxy->SetPosition(m_respawnHoldPosition);
        m_proxy->m_previousPosition = m_respawnHoldPosition;
    }

    m_isRespawning = true;
    SyncRespawnVisual();
}

void EnemyAiScript::TickRespawnHold() {
    if (!m_isRespawning) return;

    if (m_proxy) {
        m_proxy->SetActive(true);
        m_proxy->GetRigidbody().SetUseGravity(false);
        m_proxy->GetRigidbody().SetVelocity({0.0f, 0.0f, 0.0f});
        m_proxy->SetPosition(m_respawnHoldPosition);
        m_proxy->m_previousPosition = m_respawnHoldPosition;
    }

    SyncRespawnVisual();
}

void EnemyAiScript::EndRespawn() {
    if (!m_isRespawning) return;

    if (m_proxy) {
        m_proxy->SetActive(true);
        m_proxy->SetPosition(m_respawnHoldPosition);
        m_proxy->m_previousPosition = m_respawnHoldPosition;
        m_proxy->GetRigidbody().SetVelocity({0.0f, 0.0f, 0.0f});
        m_proxy->GetRigidbody().SetUseGravity(m_respawnGravityEnabled);
    }

    m_isRespawning = false;
    m_mode = EnemyDriveMode::Wander;
    m_modeTimer = 0.0f;
    m_isOrbiting = false;
    m_controlState = EnemyControlState::Normal;
    m_knockbackTimer = 0.0f;
    m_stunImmobilize = false;
    m_stunSpinAngle = 0.0f;
    m_magneticEnergy = kMagneticEnergyMax;
    m_magneticOverheated = false;
    m_magCombatCooldown = 0.0f;
    m_magUtilityCooldown = 0.0f;
    m_reactionTimer = 0.0f;
    m_intentCommitTimer = 0.0f;
    m_comboStep = 0;
    m_comboPullTimer = 0.0f;
    m_comboTarget = nullptr;
    m_justExitedKnockback = false;
    m_currentIntent = EnemyIntent::Wander;
    m_magPurpose = EnemyMagPurpose::None;
    ResolveTuning();
    m_arenaFootprintRefreshTimer = 0.0f;
    m_groundCheckTimer = 0.0f;
    m_magObstacleScanTimer = 0.0f;
    SyncMagneticEnergyUI();

    if (m_hostObject) {
        m_hostObject->SetActive(true);
        SyncEnemyTeamMaterial(*m_hostObject, GetPlayerIndex());
        ClearEnemyEmissionGlow(*m_hostObject);
        if (engine::Transform* t = ScriptApi_FindTransform(*m_hostObject)) {
            t->localPosition = m_proxy ? m_proxy->GetPosition() : m_respawnHoldPosition;
            t->localRotation.y = m_yaw;
        }
    }

    if (m_registryIndex >= 0) {
        const XMFLOAT3 registryPos = m_proxy ? m_proxy->GetPosition() : m_respawnHoldPosition;
        CenterPole_UpdateDynamicPosition(m_registryIndex, registryPos);
    }
}

void EnemyAiScript::ExitMagneticModes() {
    m_isOrbiting = false;
    m_orbitAngularVelocity = 0.0f;
    if (IsMagneticModeActive()) {
        EndMagPull(true);
        return;
    }
    m_magnetTarget = nullptr;
    m_magnetTargetScoreIndex = -1;
}

void EnemyAiScript::EndMagPull(bool cooldown) {
    const EnemyMagPurpose endedPurpose = m_magPurpose;
    m_isOrbiting = false;
    m_orbitAngularVelocity = 0.0f;
    m_magPullKnockbackApplied = false;
    m_magObstacleScanTimer = 0.0f;
    m_magnetTarget = nullptr;
    m_magnetTargetScoreIndex = -1;
    m_magPurpose = EnemyMagPurpose::None;
    m_postOrbitResumeActive = false;

    if (cooldown) {
        if (endedPurpose == EnemyMagPurpose::Utility) {
            m_magUtilityCooldown = kMagUtilityCooldown * m_tuning.utilityCooldownScale;
            m_mode = EnemyDriveMode::Wander;
            m_modeTimer = 0.0f;
        } else {
            m_mode = EnemyDriveMode::Cooldown;
            m_modeTimer = RandRange(0.35f, 0.8f);
            m_magCombatCooldown = kMagComboCooldown;
        }
    } else {
        m_mode = EnemyDriveMode::Wander;
        m_modeTimer = 0.0f;
    }
}

void EnemyAiScript::EndStunVisual(engine::GameObject& self) {
    m_stunImmobilize = false;
    m_stunSpinAngle = 0.0f;
    ClearEnemyEmissionGlow(self);
    if (engine::Transform* t = ScriptApi_FindTransform(self)) {
        t->localPosition = m_proxy->m_position;
        t->localRotation.y = m_yaw;
    }
}

void EnemyAiScript::BeginKnockback(float durationSeconds, bool immobilize) {
    const float duration = (durationSeconds > 0.0f)
        ? durationSeconds
        : (std::max)(0.05f, kKnockbackDuration);

    if (!immobilize && m_stunImmobilize && m_hostObject)
        EndStunVisual(*m_hostObject);

    m_controlState = EnemyControlState::Knockback;
    m_knockbackTimer = duration;
    m_stunImmobilize = immobilize;
    if (immobilize)
        m_stunSpinAngle = 0.0f;
    ExitMagneticModes();
    if (immobilize && m_proxy) {
        const XMFLOAT3 zero = {0.0f, 0.0f, 0.0f};
        m_proxy->GetRigidbody().SetVelocity(zero);
    }
}

void EnemyAiScript::SyncStunVisualRotation(engine::GameObject& self) {
    const float spinY = m_yaw + m_stunSpinAngle;
    if (engine::Transform* t = ScriptApi_FindTransform(self)) {
        t->localPosition = m_proxy->m_position;
        t->localRotation.y = spinY;
    }
}

void EnemyAiScript::TickKnockbackState(engine::GameObject& self, float dt) {
    if (m_stunImmobilize && m_proxy) {
        m_proxy->GetRigidbody().SetVelocity({0.0f, 0.0f, 0.0f});
        const float spinSpeed = (std::max)(0.0f, kStunSpinSpeed);
        m_stunSpinAngle += spinSpeed * dt;
        if (m_stunSpinAngle >= 360.0f)
            m_stunSpinAngle = fmodf(m_stunSpinAngle, 360.0f);

        const XMFLOAT3 pos = m_proxy->GetPosition();
        ScriptApi_BillboardAdd(
            {pos.x, pos.y + 1.1f, pos.z},
            {0.45f, 0.45f},
            {1.0f, 0.92f, 0.15f, 0.85f},
            nullptr,
            m_stunSpinAngle);

        if (auto* vc = ScriptApi_FindVisualComponent(self))
            vc->emissionGlow = {1.0f, 0.92f, 0.15f, 1.0f};

        SyncStunVisualRotation(self);
    } else if (engine::Transform* t = ScriptApi_FindTransform(self)) {
        t->localPosition = m_proxy->m_position;
        t->localRotation.y = m_yaw;
    }

    m_knockbackTimer -= dt;
    if (m_knockbackTimer <= 0.0f) {
        m_controlState = EnemyControlState::Normal;
        m_knockbackTimer = 0.0f;
        m_stunImmobilize = false;
        m_stunSpinAngle = 0.0f;
        m_justExitedKnockback = true;
        if (engine::Transform* t = ScriptApi_FindTransform(self)) {
            t->localPosition = m_proxy->m_position;
            t->localRotation.y = m_yaw;
        }
    }

    CenterPole_UpdateDynamicPosition(m_registryIndex, m_proxy->m_position);
}

// ---- Ground / impact ----------------------------------------------------------

void EnemyAiScript::CheckGroundStatus() {
    m_isGrounded = false;
    if (!m_proxy || !m_proxy->GetCollider()) return;

    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (!scene) return;

    Collider* myCol = m_proxy->GetCollider();
    XMFLOAT3 myPos = m_proxy->GetPosition();
    float myHalfHeight = 0.5f;
    if (myCol->GetType() == ColliderType::BOX)
        myHalfHeight = ScriptApi_GetBoxColliderSize(*static_cast<BoxCollider*>(myCol)).y * 0.5f;

    for (auto* other : scene->GetObjects()) {
        if (!other || other == m_proxy || !other->IsActive()) continue;
        Collider* otherCol = other->GetCollider();
        if (!otherCol || !myCol->CheckCollision(otherCol)) continue;

        XMFLOAT3 otherPos = ScriptApi_GetColliderWorldCenter(*otherCol);
        if (myPos.y - otherPos.y > (myHalfHeight - 0.1f)) {
            m_isGrounded = true;
            break;
        }
    }
}

void EnemyAiScript::TickGroundStatus(float dt) {
    m_groundCheckTimer -= dt;
    if (m_groundCheckTimer > 0.0f)
        return;

    CheckGroundStatus();
    m_groundCheckTimer = RandRange(kGroundCheckMinInterval, kGroundCheckMaxInterval);
}

void EnemyAiScript::HandlePlayerImpact(GameObject* player, float dt) {
    (void)dt;
    if (!m_proxy || !player) return;

    Collider* myCol = m_proxy->GetCollider();
    Collider* playerCol = player->GetCollider();
    if (!myCol || !playerCol) {
        m_playerContactSet.clear();
        return;
    }

    if (!myCol->CheckCollision(playerCol)) {
        m_playerContactSet.erase(player);
        return;
    }

    if (m_playerContactSet.count(player)) return;
    m_playerContactSet.insert(player);

    if (PlayerScript_WasChargeImpactAppliedToBody(m_proxy))
        return;

    XMFLOAT3 dir = {
        player->GetPosition().x - m_proxy->GetPosition().x,
        0.0f,
        player->GetPosition().z - m_proxy->GetPosition().z
    };
    float distSq = dir.x * dir.x + dir.z * dir.z;
    if (distSq > 0.001f) {
        float inv = 1.0f / sqrtf(distSq);
        dir.x *= inv;
        dir.z *= inv;
    } else {
        dir = {1.0f, 0.0f, 0.0f};
    }

    player->GetRigidbody().AddForce(
        {dir.x * 6.0f, 0.35f, dir.z * 6.0f},
        ForceMode::Impulse);
    m_proxy->GetRigidbody().AddForce(
        {-dir.x * 4.0f, 0.0f, -dir.z * 4.0f},
        ForceMode::Impulse);
    BeginKnockback(-1.0f, false);
}

// ---- Wander -----------------------------------------------------------------

void EnemyAiScript::TickWanderDirection(float dt) {
    m_wanderTimer -= dt;
    if (m_wanderTimer > 0.0f) return;

    float baseDelay = m_tuning.wanderBaseDelay;
    float blunderChance = m_tuning.wanderBlunderChance;
    if (IsNearArenaEdge())
        blunderChance *= 0.35f;

    if (Rand01() < blunderChance) {
        m_wanderDirX = 0.0f;
        m_wanderDirZ = 0.0f;
    } else {
        for (int attempt = 0; attempt < 6; ++attempt) {
            float angle = Rand01() * 6.2831853f;
            const float dirX = cosf(angle);
            const float dirZ = sinf(angle);
            if (IsDirectionTowardArenaCenter(dirX, dirZ)) {
                m_wanderDirX = dirX;
                m_wanderDirZ = dirZ;
                break;
            }
            if (attempt == 5) {
                const XMFLOAT3 pos = m_proxy->GetPosition();
                float dx = m_arenaCenter.x - pos.x;
                float dz = m_arenaCenter.z - pos.z;
                const float len = sqrtf(dx * dx + dz * dz);
                if (len > 0.01f) {
                    m_wanderDirX = dx / len;
                    m_wanderDirZ = dz / len;
                } else {
                    m_wanderDirX = dirX;
                    m_wanderDirZ = dirZ;
                }
            }
        }
    }

    m_wanderTimer = baseDelay / ProfileAggro();
}

void EnemyAiScript::TickPathVariation(float dt) {
    m_pathVariationTimer -= dt;
    if (m_pathVariationTimer > 0.0f) return;

    m_pathSide = Rand01() < 0.5f ? -1.0f : 1.0f;
    m_pathStrength = RandRange(0.12f, 0.42f);
    if (IsNearArenaEdge())
        m_pathStrength *= 0.35f;
    m_pathVariationTimer = RandRange(kPathVariationMinDelay, kPathVariationMaxDelay) / ProfileAggro();
}

// ---- Goal picking -------------------------------------------------------------

static float DistSqXZ(const XMFLOAT3& a, const XMFLOAT3& b) {
    float dx = a.x - b.x;
    float dz = a.z - b.z;
    return dx * dx + dz * dz;
}

// ---- Intent scoring brain -----------------------------------------------------

static float DistXZ(const XMFLOAT3& a, const XMFLOAT3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return sqrtf(dx * dx + dz * dz);
}

static float Saturate01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

bool EnemyAiScript::IsIntentCommitted() const {
    return m_intentCommitTimer > 0.0f;
}

bool EnemyAiScript::CanRethinkIntent() const {
    if (IsIntentCommitted()) return false;
    if (IsMagPullActive()) return false;
    if (m_mode == EnemyDriveMode::ChasePlace && HasHeldItem()) return false;
    if (m_reactionTimer > 0.0f) return false;
    return true;
}

void EnemyAiScript::RecordMagUse(bool combat) {
    if (combat) {
        ++m_recentCombatMagUses;
        m_magFatigueTimer = kMagFatigueWindow;
    }
}

EnemyAiContext EnemyAiScript::BuildContext(GameObject* player) const {
    EnemyAiContext ctx;
    if (!m_proxy) return ctx;

    const XMFLOAT3 selfPos = m_proxy->GetPosition();
    ctx.energyRatio = m_magneticEnergy / (std::max)(0.01f, kMagneticEnergyMax);
    ctx.nearEdge = IsNearArenaEdge();
    ctx.utilityCdClear = m_magUtilityCooldown <= 0.0f;
    ctx.combatCdClear = m_magCombatCooldown <= 0.0f;
    ctx.chargeCdClear = m_chargeCooldownRemaining <= 0.0f;
    ctx.canUseMagnetic = CanUseMagnetic();
    ctx.utilityEnergyOk = ctx.energyRatio >= m_tuning.magUtilityEnergyMin;
    ctx.combatFatigued = m_recentCombatMagUses >= 2;
    ctx.justExitedKnockback = m_justExitedKnockback;

    ctx.attackTarget = SelectAttackTarget(&ctx.attackScoreIndex);
    if (!ctx.attackTarget)
        ctx.attackTarget = FallbackOpponentPlayer(this);

    if (ctx.attackTarget) {
        ctx.distToTarget = DistXZ(selfPos, ctx.attackTarget->GetPosition());
        ctx.targetCharging = IsTargetChargingToward(ctx.attackTarget);
    }

    if (!HasHeldItem()) {
        ctx.pickup = ItemWorld_FindNearestWorldPickup(selfPos, kItemSeekRadius, nullptr);
    }

    ctx.hasBonusZone = PointArea_GetCenter(ctx.bonusCenter);
    if (ctx.hasBonusZone)
        ctx.insideBonusZone = PointArea_IsInside(selfPos);

    const int myScore = GameScore_Get(GetPlayerIndex());
    int bestOtherScore = -1;
    EnemyCompetitorBuffer competitors;
    AddCompetitors(competitors, this);
    for (int i = 0; i < competitors.count; ++i) {
        const EnemyCompetitor& c = competitors.entries[i];
        if (c.scoreIndex == GetPlayerIndex()) continue;
        bestOtherScore = (std::max)(bestOtherScore, GameScore_Get(c.scoreIndex));
    }
    ctx.leading = bestOtherScore >= 0 &&
        myScore >= bestOtherScore + static_cast<int>(kScoreLeadThreshold);
    ctx.behind = bestOtherScore >= 0 &&
        myScore + static_cast<int>(kScoreLeadThreshold) < bestOtherScore;

    if (HasHeldItem() && ctx.attackTarget) {
        ctx.goalPos = ctx.attackTarget->GetPosition();
        ctx.distToGoal = ctx.distToTarget;
    } else if (ctx.pickup) {
        ctx.goalPos = ctx.pickup->GetPosition();
        ctx.distToGoal = DistXZ(selfPos, ctx.goalPos);
    } else if (ctx.hasBonusZone && !ctx.insideBonusZone) {
        ctx.goalPos = ctx.bonusCenter;
        ctx.distToGoal = DistXZ(selfPos, ctx.bonusCenter);
    } else if (ctx.attackTarget) {
        ctx.goalPos = ctx.attackTarget->GetPosition();
        ctx.distToGoal = ctx.distToTarget;
    }

    if (ctx.distToGoal > m_tuning.magTravelMinDist) {
        ctx.travelPole = SelectPoleAssistTarget(ctx.goalPos);
        ctx.poleAvailable = ctx.travelPole != nullptr;
    }

    if (m_hasArenaFootprint) {
        float dx = m_arenaCenter.x - selfPos.x;
        float dz = m_arenaCenter.z - selfPos.z;
        const float len = sqrtf(dx * dx + dz * dz);
        if (len > 0.01f) {
            ctx.arenaCenterDir = {dx / len, 0.0f, dz / len};
            if (ctx.nearEdge && !ctx.poleAvailable)
                ctx.travelPole = SelectPoleAssistTarget(m_arenaCenter);
            ctx.poleAvailable = ctx.poleAvailable || ctx.travelPole != nullptr;
        }
    }

    return ctx;
}

float EnemyAiScript::ScoreIntent(EnemyIntent intent, const EnemyAiContext& ctx) const {
    const float aggro = ProfileAggro();
    float score = 0.0f;

    switch (intent) {
        case EnemyIntent::ChasePlace:
            if (HasHeldItem() && ctx.attackTarget)
                score = 0.95f;
            break;

        case EnemyIntent::ChaseItem:
            if (!HasHeldItem() && ctx.pickup) {
                score = m_tuning.itemWeight * m_tuning.itemChaseChance * aggro;
                if (ctx.distToGoal > m_tuning.magTravelMinDist)
                    score *= 0.85f;
            }
            break;

        case EnemyIntent::ChaseBonus:
            if (ctx.hasBonusZone && !ctx.insideBonusZone) {
                score = m_tuning.bonusWeight * aggro;
                if (ctx.behind) score *= 1.85f;
                if (ctx.leading) score *= 0.45f;
            }
            break;

        case EnemyIntent::MagTravel:
            if (ctx.canUseMagnetic && ctx.utilityEnergyOk && ctx.utilityCdClear &&
                ctx.poleAvailable && ctx.distToGoal > m_tuning.magTravelMinDist) {
                score = 0.22f + Saturate01((ctx.distToGoal - m_tuning.magTravelMinDist) / 18.0f) * 0.35f;
                score *= m_tuning.utilityMagnetWeight;
                if (ctx.behind) score *= 1.25f;
            }
            break;

        case EnemyIntent::MagRetreat:
            // Push = self-repulse away from target (spacing/flee), never an attack.
            if (ctx.canUseMagnetic && ctx.utilityEnergyOk && ctx.utilityCdClear) {
                if (ctx.justExitedKnockback)
                    score = 0.72f;
                else if (ctx.nearEdge && ctx.energyRatio < 0.55f)
                    score = 0.58f;
                else if (ctx.nearEdge)
                    score = 0.42f;

                if (ctx.attackTarget && !HasHeldItem() &&
                    ctx.distToTarget <= kMagnetMaxDist) {
                    float spaceScore = 0.0f;
                    if (ctx.targetCharging)
                        spaceScore = 0.68f;
                    else if (ctx.distToTarget < 8.0f)
                        spaceScore = 0.48f;
                    else if (ctx.distToTarget < kMagAttackMidDist)
                        spaceScore = 0.32f;
                    spaceScore *= m_tuning.utilityMagnetWeight;
                    if (spaceScore > score)
                        score = spaceScore;
                }
            }
            break;

        case EnemyIntent::MagAttackPull:
            if (ctx.attackTarget && ctx.canUseMagnetic && ctx.combatCdClear &&
                !HasHeldItem() && ctx.distToTarget <= kMagnetMaxDist) {
                if (ctx.distToTarget >= kMagAttackMidDist && ctx.distToTarget <= 25.0f)
                    score = m_tuning.combatWeight * m_tuning.playerAttackChance * aggro * 0.95f;
                else if (ctx.distToTarget < kMagAttackMidDist)
                    score = m_tuning.combatWeight * m_tuning.playerAttackChance * aggro * 0.55f;
                if (ctx.leading) score *= 1.65f;
                if (ctx.behind) score *= 0.55f;
                if (ctx.combatFatigued) score *= kMagFatigueCombatMultiplier;
            }
            break;

        case EnemyIntent::MagAttackOrbit:
            if (ctx.attackTarget && ctx.canUseMagnetic && ctx.combatCdClear &&
                !HasHeldItem() && ctx.distToTarget <= kMagnetMaxDist) {
                score = m_tuning.combatWeight * m_tuning.playerAttackChance * aggro * 0.40f;
                if (ctx.distToTarget > kMagAttackFarDist) score += 0.35f;
                else if (ctx.distToTarget > kMagAttackMidDist) score += 0.18f;
                score *= m_tuning.comboMultiplier;
                if (ctx.combatFatigued) score *= kMagFatigueCombatMultiplier;
            }
            break;

        case EnemyIntent::ChargeAttack:
            if (ctx.attackTarget && ctx.chargeCdClear && !HasHeldItem() &&
                ctx.distToTarget <= kChargeMaxDist && IsPlayerBody(ctx.attackTarget) &&
                IsAimedAtPlayerForCharge(ctx.attackTarget)) {
                score = m_tuning.chargeChance * aggro;
                if (ctx.distToTarget >= kMagAttackMidDist && ctx.distToTarget <= 22.0f)
                    score += 0.22f;
                if (ctx.leading) score *= 1.25f;
            }
            break;

        case EnemyIntent::Wander:
        default:
            score = 0.15f + m_tuning.wanderBlunderChance * 0.35f;
            if (!ctx.attackTarget && !ctx.pickup && !ctx.hasBonusZone)
                score = 0.55f;
            break;
    }

    return Saturate01(score);
}

EnemyIntent EnemyAiScript::PickIntent(const EnemyAiContext& ctx) {
    EnemyIntent bestIntent = EnemyIntent::Wander;
    float bestScore = -1.0f;
    const float noise = m_tuning.intentNoise;

    for (int i = 0; i < static_cast<int>(EnemyIntent::Count); ++i) {
        const EnemyIntent intent = static_cast<EnemyIntent>(i);
        const float baseScore = ScoreIntent(intent, ctx);
        const float noisyScore = baseScore + RandRange(-noise, noise);
        m_debugIntentScores[i] = baseScore;
        if (noisyScore > bestScore) {
            bestScore = noisyScore;
            bestIntent = intent;
        }
    }

    if (bestScore < 0.12f)
        return EnemyIntent::Wander;
    return bestIntent;
}

void EnemyAiScript::QueueIntent(EnemyIntent intent) {
    if (intent == m_currentIntent && m_reactionTimer <= 0.0f)
        return;

    m_pendingIntent = intent;
    float delay = RandRange(m_tuning.reactionDelayMin, m_tuning.reactionDelayMax) / ProfileAggro();
    if ((intent == EnemyIntent::MagAttackPull || intent == EnemyIntent::ChargeAttack) &&
        Rand01() < m_tuning.hesitationChance) {
        delay += 0.30f;
    }
    m_reactionTimer = delay;
}

void EnemyAiScript::ApplyIntent(EnemyIntent intent, const EnemyAiContext& ctx) {
    const float aggro = ProfileAggro();
    m_currentIntent = intent;
    m_postOrbitResumeActive = false;
    m_magPurpose = EnemyMagPurpose::None;

    switch (intent) {
        case EnemyIntent::ChasePlace:
            if (ctx.attackTarget) {
                m_mode = EnemyDriveMode::ChasePlace;
                m_chaseTarget = ctx.attackTarget->GetPosition();
                m_magnetTarget = ctx.attackTarget;
                m_magnetTargetScoreIndex = ctx.attackScoreIndex;
                m_modeTimer = 60.0f;
                m_usedMidChaseTravel = false;
            } else {
                m_mode = EnemyDriveMode::Wander;
                m_modeTimer = 0.0f;
            }
            break;

        case EnemyIntent::ChaseItem:
            if (ctx.pickup) {
                m_mode = EnemyDriveMode::ChaseItem;
                m_itemChaseTarget = ctx.pickup;
                m_chaseTarget = ctx.pickup->GetPosition();
                m_modeTimer = RandRange(6.0f, 10.0f);
                m_usedMidChaseTravel = false;
            } else {
                m_mode = EnemyDriveMode::Wander;
                m_modeTimer = 0.0f;
            }
            break;

        case EnemyIntent::ChaseBonus:
            m_mode = EnemyDriveMode::ChaseBonus;
            m_chaseTarget = ctx.bonusCenter;
            m_modeTimer = RandRange(5.0f, 9.0f);
            m_usedMidChaseTravel = false;
            break;

        case EnemyIntent::MagTravel:
            if (ctx.travelPole) {
                const EnemyDriveMode previousMode = m_mode;
                m_mode = EnemyDriveMode::MagOrbit;
                m_magnetTarget = ctx.travelPole;
                m_magnetTargetScoreIndex = -1;
                m_magPurpose = EnemyMagPurpose::Utility;
                m_chaseTarget = ctx.goalPos;
                m_modeTimer = RandRange(0.55f, 1.25f);
                m_isOrbiting = false;
                m_orbitAngularVelocity = 0.0f;
                m_orbitSpinSign = Rand01() < 0.5f ? -1 : 1;
                if (previousMode == EnemyDriveMode::ChaseItem ||
                    previousMode == EnemyDriveMode::ChaseBonus ||
                    previousMode == EnemyDriveMode::ChasePlace) {
                    m_postOrbitResumeMode = previousMode;
                    m_postOrbitResumeActive = true;
                } else if (ctx.pickup) {
                    m_postOrbitResumeMode = EnemyDriveMode::ChaseItem;
                    m_postOrbitResumeActive = true;
                    m_itemChaseTarget = ctx.pickup;
                } else if (ctx.hasBonusZone && !ctx.insideBonusZone) {
                    m_postOrbitResumeMode = EnemyDriveMode::ChaseBonus;
                    m_postOrbitResumeActive = true;
                } else if (HasHeldItem()) {
                    m_postOrbitResumeMode = EnemyDriveMode::ChasePlace;
                    m_postOrbitResumeActive = true;
                }
                RecordMagUse(false);
                m_intentCommitTimer = RandRange(0.5f, 1.0f);
            } else {
                ApplyIntent(EnemyIntent::Wander, ctx);
            }
            break;

        case EnemyIntent::MagRetreat: {
            const bool canSelfPush =
                ctx.attackTarget && ctx.canUseMagnetic &&
                ctx.distToTarget <= kMagnetMaxDist &&
                (ctx.distToTarget < 8.0f || ctx.targetCharging);
            if (canSelfPush) {
                // Self-repulse away from target — spacing/flee, not damage.
                m_mode = EnemyDriveMode::MagPush;
                m_magnetTarget = ctx.attackTarget;
                m_magnetTargetScoreIndex = ctx.attackScoreIndex;
                m_magPurpose = EnemyMagPurpose::Utility;
                m_modeTimer = RandRange(0.45f, 0.85f);
                RecordMagUse(false);
                m_intentCommitTimer = RandRange(0.45f, 0.8f);
            } else if (ctx.travelPole) {
                m_mode = EnemyDriveMode::MagOrbit;
                m_magnetTarget = ctx.travelPole;
                m_magnetTargetScoreIndex = -1;
                m_magPurpose = EnemyMagPurpose::Utility;
                m_chaseTarget = m_hasArenaFootprint ? m_arenaCenter : ctx.goalPos;
                m_modeTimer = RandRange(0.45f, 0.95f);
                m_isOrbiting = false;
                m_orbitAngularVelocity = 0.0f;
                m_orbitSpinSign = Rand01() < 0.5f ? -1 : 1;
                m_postOrbitResumeMode = EnemyDriveMode::Wander;
                m_postOrbitResumeActive = true;
                RecordMagUse(false);
                m_intentCommitTimer = RandRange(0.45f, 0.8f);
            } else {
                m_mode = EnemyDriveMode::Wander;
                m_modeTimer = 0.0f;
            }
            break;
        }

        case EnemyIntent::MagAttackPull:
            m_mode = EnemyDriveMode::MagAttach;
            m_magnetTarget = ctx.attackTarget;
            m_magnetTargetScoreIndex = ctx.attackScoreIndex;
            m_magPurpose = EnemyMagPurpose::Combat;
            m_modeTimer = RandRange(1.8f, 3.2f) / aggro;
            m_magPullKnockbackApplied = false;
            RecordMagUse(true);
            m_intentCommitTimer = RandRange(0.6f, 1.2f);
            break;

        case EnemyIntent::MagAttackOrbit:
            m_mode = EnemyDriveMode::MagOrbit;
            m_magnetTarget = ctx.attackTarget;
            m_magnetTargetScoreIndex = ctx.attackScoreIndex;
            m_magPurpose = EnemyMagPurpose::Combat;
            m_modeTimer = RandRange(0.65f, 1.35f) / aggro;
            m_isOrbiting = false;
            m_orbitAngularVelocity = 0.0f;
            m_orbitSpinSign = Rand01() < 0.5f ? -1 : 1;
            m_comboTarget = ctx.attackTarget;
            RecordMagUse(true);
            m_intentCommitTimer = RandRange(0.55f, 1.0f);
            break;

        case EnemyIntent::ChargeAttack:
            m_mode = EnemyDriveMode::ChargeKnockback;
            m_magnetTarget = ctx.attackTarget;
            m_magnetTargetScoreIndex = ctx.attackScoreIndex;
            m_modeTimer = kChargeAttackTimeout;
            m_intentCommitTimer = kChargeAttackTimeout;
            break;

        case EnemyIntent::Wander:
        default:
            m_mode = EnemyDriveMode::Wander;
            m_modeTimer = 0.0f;
            break;
    }

    if (IsMagneticModeActive()) {
        m_isOrbiting = m_mode == EnemyDriveMode::MagOrbit ? m_isOrbiting : false;
        if (m_mode == EnemyDriveMode::MagOrbit && !m_isOrbiting)
            m_orbitAngularVelocity = 0.0f;
    }
}

void EnemyAiScript::TryMidChaseMagTravel() {
    if (!m_proxy || !CanUseMagnetic() || m_magUtilityCooldown > 0.0f)
        return;
    if (m_magneticEnergy / (std::max)(0.01f, kMagneticEnergyMax) < m_tuning.magUtilityEnergyMin)
        return;
    if (IsIntentCommitted() || IsMagneticModeActive() || m_usedMidChaseTravel)
        return;
    if (Rand01() > m_tuning.midChaseTravelChance)
        return;

    XMFLOAT3 goal = m_chaseTarget;
    float dist = DistXZ(m_proxy->GetPosition(), goal);
    if (dist <= m_tuning.magTravelMinDist)
        return;

    GameObject* pole = SelectPoleAssistTarget(goal);
    if (!pole) return;

    m_usedMidChaseTravel = true;
    m_postOrbitResumeMode = m_mode;
    m_postOrbitResumeActive = true;
    m_mode = EnemyDriveMode::MagOrbit;
    m_magnetTarget = pole;
    m_magnetTargetScoreIndex = -1;
    m_magPurpose = EnemyMagPurpose::Utility;
    m_modeTimer = RandRange(0.55f, 1.15f);
    m_isOrbiting = false;
    m_orbitAngularVelocity = 0.0f;
    m_orbitSpinSign = Rand01() < 0.5f ? -1 : 1;
    m_currentIntent = EnemyIntent::MagTravel;
    m_intentCommitTimer = RandRange(0.5f, 0.9f);
    RecordMagUse(false);
}

void EnemyAiScript::OnOrbitLaunchEnd() {
    const EnemyMagPurpose endedPurpose = m_magPurpose;
    const bool wasUtility = endedPurpose == EnemyMagPurpose::Utility;
    GameObject* savedComboTarget = m_comboTarget;

    m_isOrbiting = false;
    m_orbitAngularVelocity = 0.0f;
    m_magObstacleScanTimer = 0.0f;

    if (wasUtility && m_postOrbitResumeActive) {
        m_mode = m_postOrbitResumeMode;
        m_modeTimer = (m_postOrbitResumeMode == EnemyDriveMode::ChasePlace)
            ? 60.0f
            : RandRange(5.0f, 9.0f);
        m_magnetTarget = nullptr;
        m_magnetTargetScoreIndex = -1;
        m_magPurpose = EnemyMagPurpose::None;
        m_postOrbitResumeActive = false;
        m_magUtilityCooldown = kMagUtilityCooldown * m_tuning.utilityCooldownScale;
        return;
    }

    if (wasUtility) {
        m_mode = EnemyDriveMode::Wander;
        m_modeTimer = 0.0f;
        m_magnetTarget = nullptr;
        m_magnetTargetScoreIndex = -1;
        m_magPurpose = EnemyMagPurpose::None;
        m_magUtilityCooldown = kMagUtilityCooldown * m_tuning.utilityCooldownScale;
        return;
    }

    if (savedComboTarget && savedComboTarget->IsActive() &&
        Rand01() < m_tuning.comboChainChance * m_tuning.comboMultiplier) {
        m_comboStep = 1;
        m_comboTarget = savedComboTarget;
        m_comboPullTimer = kComboPullWindow;
        m_magnetTarget = nullptr;
        m_magnetTargetScoreIndex = -1;
        m_magPurpose = EnemyMagPurpose::None;
        m_mode = EnemyDriveMode::Wander;
        m_modeTimer = 0.0f;
        m_magCombatCooldown = kMagComboCooldown;
        return;
    }

    EndMagPull(true);
}

void EnemyAiScript::CheckComboTransitions(GameObject* target, float dt) {
    if (m_comboStep != 1 || !target || !target->IsActive()) {
        m_comboStep = 0;
        m_comboTarget = nullptr;
        m_comboPullTimer = 0.0f;
        return;
    }

    m_comboPullTimer -= dt;
    if (m_comboPullTimer <= 0.0f) {
        m_comboStep = 0;
        m_comboTarget = nullptr;
        return;
    }

    if (!CanUseMagnetic() || m_magCombatCooldown > 0.0f)
        return;

    const float distSq = DistSqXZBetweenObjects(m_proxy, target);
    if (distSq > kMagnetMaxDist * kMagnetMaxDist)
        return;

    m_comboStep = 0;
    m_mode = EnemyDriveMode::MagAttach;
    m_magnetTarget = target;
    m_magPurpose = EnemyMagPurpose::Combat;
    m_modeTimer = RandRange(1.4f, 2.4f) / ProfileAggro();
    m_magPullKnockbackApplied = false;
    m_currentIntent = EnemyIntent::MagAttackPull;
    m_intentCommitTimer = RandRange(0.5f, 0.9f);
    RecordMagUse(true);
}

void EnemyAiScript::TickIntentSystem(GameObject* player, float dt) {
    if (m_magFatigueTimer > 0.0f) {
        m_magFatigueTimer -= dt;
        if (m_magFatigueTimer <= 0.0f)
            m_recentCombatMagUses = 0;
    }

    if (m_intentCommitTimer > 0.0f)
        m_intentCommitTimer -= dt;

    if (m_reactionTimer > 0.0f) {
        m_reactionTimer -= dt;
        if (m_reactionTimer <= 0.0f) {
            EnemyAiContext ctx = BuildContext(player);
            ApplyIntent(m_pendingIntent, ctx);
        }
        return;
    }

    m_rethinkTimer -= dt;
    if (m_rethinkTimer > 0.0f || !CanRethinkIntent())
        return;

    const bool chasingBonus = (m_mode == EnemyDriveMode::ChaseBonus && m_modeTimer > 0.0f);
    const bool chasingItem  = (m_mode == EnemyDriveMode::ChaseItem && m_itemChaseTarget);
    const bool chasingPlace = (m_mode == EnemyDriveMode::ChasePlace && HasHeldItem());
    if (chasingBonus || chasingItem || chasingPlace)
        return;

    if (m_mode == EnemyDriveMode::Cooldown)
        return;

    EnemyAiContext ctx = BuildContext(player);
    const EnemyIntent picked = PickIntent(ctx);
    if (picked != m_currentIntent)
        QueueIntent(picked);

    m_rethinkTimer = RandRange(0.35f, 0.95f) / ProfileAggro();
}

void EnemyAiScript::TryPickNewGoal(GameObject* player) {
    if (!CanRethinkIntent())
        return;

    EnemyAiContext ctx = BuildContext(player);
    const EnemyIntent picked = PickIntent(ctx);
    QueueIntent(picked);
    (void)player;
}

// ---- Movement per mode ------------------------------------------------------

void EnemyAiScript::ComputeMoveForMode(GameObject* player) {
    m_moveX = 0.0f;
    m_moveZ = 0.0f;
    bool applyPathVariation = false;

    switch (m_mode) {
        case EnemyDriveMode::Wander:
            m_moveX = m_wanderDirX;
            m_moveZ = m_wanderDirZ;
            break;
        case EnemyDriveMode::Cooldown:
            m_moveX = m_wanderDirX * 0.65f;
            m_moveZ = m_wanderDirZ * 0.65f;
            break;
        case EnemyDriveMode::ChaseBonus:
        case EnemyDriveMode::ChaseItem:
        case EnemyDriveMode::ChasePlace: {
            float dx = m_chaseTarget.x - m_proxy->m_position.x;
            float dz = m_chaseTarget.z - m_proxy->m_position.z;
            float distSq = dx * dx + dz * dz;
            if (distSq <= kBonusArriveSqr) break;
            float inv = 1.0f / sqrtf(distSq);
            m_moveX = dx * inv;
            m_moveZ = dz * inv;
            applyPathVariation = distSq > 6.0f * 6.0f;
            break;
        }
        case EnemyDriveMode::MagAttach:
            if (GameObject* target = m_magnetTarget ? m_magnetTarget : player) {
                float dx = target->GetPosition().x - m_proxy->m_position.x;
                float dz = target->GetPosition().z - m_proxy->m_position.z;
                float distSq = dx * dx + dz * dz;
                if (distSq > 0.01f) {
                    float inv = 1.0f / sqrtf(distSq);
                    m_moveX = dx * inv;
                    m_moveZ = dz * inv;
                    applyPathVariation = distSq > 5.0f * 5.0f;
                }
            }
            break;
        case EnemyDriveMode::ChargeKnockback:
            if (GameObject* target = m_magnetTarget ? m_magnetTarget : player) {
                float dx = target->GetPosition().x - m_proxy->m_position.x;
                float dz = target->GetPosition().z - m_proxy->m_position.z;
                float distSq = dx * dx + dz * dz;
                if (distSq > 0.01f) {
                    float inv = 1.0f / sqrtf(distSq);
                    m_moveX = dx * inv;
                    m_moveZ = dz * inv;
                }
            }
            break;
        case EnemyDriveMode::MagPush:
        case EnemyDriveMode::MagOrbit:
            break;
        default:
            break;
    }

    if (applyPathVariation && m_pathStrength > 0.001f) {
        const float sideX = -m_moveZ * m_pathSide;
        const float sideZ = m_moveX * m_pathSide;
        m_moveX += sideX * m_pathStrength;
        m_moveZ += sideZ * m_pathStrength;
    }

    SteerMoveTowardSafeXZ(m_moveX, m_moveZ);

    if (m_moveX != 0.0f || m_moveZ != 0.0f) {
        float len = sqrtf(m_moveX * m_moveX + m_moveZ * m_moveZ);
        if (len > 0.01f) {
            m_moveX /= len;
            m_moveZ /= len;
            m_yaw = atan2f(m_moveX, m_moveZ) * (180.0f / 3.14159265f);
        }
    }
}

// ---- Magnet forces ----------------------------------------------------------

static void ApplyMagnetToTarget(GameObject& self, const XMFLOAT3& targetPos,
                                float force, float maxDist, float sign) {
    float dx = targetPos.x - self.GetPosition().x;
    float dz = targetPos.z - self.GetPosition().z;
    float dist = sqrtf(dx * dx + dz * dz);
    if (dist < 0.1f || dist > maxDist) return;
    float inv = sign / dist;
    self.GetRigidbody().AddForce(
        {dx * inv * force, 0.0f, dz * inv * force},
        ForceMode::Acceleration);
}

float EnemyAiScript::ComputeMagPullTouchDist(GameObject* target) const {
    if (!m_proxy || !target)
        return kMagPullTouchDistFallback;

    Collider* myCol = m_proxy->GetCollider();
    Collider* targetCol = target->GetCollider();
    if (!myCol || !targetCol)
        return kMagPullTouchDistFallback;

    (void)myCol;
    (void)targetCol;
    return ColliderContactReachXZ(m_proxy) + ColliderContactReachXZ(target) + kMagPullTouchMargin;
}

bool EnemyAiScript::IsTouchingTarget(GameObject* target) const {
    if (!m_proxy || !target) return false;

    Collider* myCol = m_proxy->GetCollider();
    Collider* targetCol = target->GetCollider();
    if (myCol && targetCol && myCol->CheckCollision(targetCol))
        return true;

    const float touchDist = ComputeMagPullTouchDist(target);
    const float touchDistSq = touchDist * touchDist;
    return DistSqXZBetweenObjects(m_proxy, target) <= touchDistSq;
}

bool EnemyAiScript::TryFinishMagPullPlayerHit(GameObject* target) {
    if (!m_proxy || !target || !IsMagPullActive()) return false;
    if (!IsTouchingTarget(target)) return false;

    XMFLOAT3 toTarget = {
        target->GetPosition().x - m_proxy->GetPosition().x,
        0.0f,
        target->GetPosition().z - m_proxy->GetPosition().z
    };
    float toLenSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z;
    if (toLenSq > 0.0001f) {
        const float inv = 1.0f / sqrtf(toLenSq);
        toTarget.x *= inv;
        toTarget.z *= inv;

        XMFLOAT3 vel = m_proxy->GetRigidbody().GetVelocity();
        vel.y = 0.0f;
        const float approach = vel.x * toTarget.x + vel.z * toTarget.z;
        if (approach > 0.0f) {
            vel.x -= toTarget.x * approach;
            vel.z -= toTarget.z * approach;
            m_proxy->GetRigidbody().SetVelocity(vel);
        }
    }

    if (!m_magPullKnockbackApplied)
        TryApplyMagPullPlayerKnockback(target);

    return true;
}

bool EnemyAiScript::TryApplyMagPullPlayerKnockback(GameObject* target) {
    if (!m_proxy || !target || m_magPullKnockbackApplied) return false;

    XMFLOAT3 dir = {
        target->GetPosition().x - m_proxy->GetPosition().x,
        0.0f,
        target->GetPosition().z - m_proxy->GetPosition().z
    };
    float distSq = dir.x * dir.x + dir.z * dir.z;
    if (distSq > 0.001f) {
        const float inv = 1.0f / sqrtf(distSq);
        dir.x *= inv;
        dir.z *= inv;
    } else {
        dir = {0.0f, 0.0f, 1.0f};
    }

    EndMagPull(true);

    if (IsPlayerBody(target)) {
        PlayerScript_ApplyChargeKnockbackToBody(
            target, dir, kChargeImpactForce, kChargeImpactLift, kChargeKnockbackDuration);
    } else {
        EnemyAi_ApplyDirectionalImpulseToProxy(
            target, dir, kChargeImpactForce, kChargeImpactLift, kChargeKnockbackDuration);
    }

    const float recoil = kChargeSelfRecoilBaseImpulse * kChargeRecoilScale;
    m_proxy->GetRigidbody().AddForce(
        {-dir.x * recoil, kChargeSelfRecoilLiftScale * recoil, -dir.z * recoil},
        ForceMode::Impulse);
    BeginKnockback(kChargeKnockbackDuration, false);
    m_chargeCooldownRemaining = kChargeCooldown;
    m_magPullKnockbackApplied = true;
    return true;
}

bool EnemyAiScript::TickMagPullCollisionStop(GameObject* target, float dt) {
    if (!m_proxy) return false;

    Collider* myCol = m_proxy->GetCollider();
    if (!myCol) return false;

    if (target) {
        Collider* targetCol = target->GetCollider();
        if (targetCol && myCol->CheckCollision(targetCol))
            return TryFinishMagPullPlayerHit(target);
        if (IsTouchingTarget(target))
            return TryFinishMagPullPlayerHit(target);
    }

    m_magObstacleScanTimer -= dt;
    if (m_magObstacleScanTimer > 0.0f)
        return false;
    m_magObstacleScanTimer = kMagObstacleScanInterval;

    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (!scene) return false;

    for (GameObject* other : scene->GetObjects()) {
        if (!other || other == m_proxy || !other->IsActive()) continue;
        if (!IsMagPullBlockingObstacle(other, m_proxy, m_hostObject)) continue;

        Collider* otherCol = other->GetCollider();
        if (!otherCol || !myCol->CheckCollision(otherCol)) continue;

        EndMagPull(true);
        return true;
    }

    return false;
}

void EnemyAiScript::TickMagPull(GameObject* target, float dt) {
    if (!target || !target->IsActive()) {
        EndMagPull(true);
        return;
    }

    if (!CanUseMagnetic()) {
        EndMagPull(true);
        return;
    }

    if (TryFinishMagPullPlayerHit(target))
        return;

    m_modeTimer -= dt;

    if (!IsTouchingTarget(target))
        ApplyMagnetToTarget(*m_proxy, target->GetPosition(), kMagnetForce, kMagnetMaxDist, +1.0f);

    if (TickMagPullCollisionStop(target, dt))
        return;

    if (m_modeTimer <= 0.0f)
        EndMagPull(true);
}

void EnemyAiScript::TickMagPush(GameObject* target, float dt) {
    if (!target || !target->IsActive() || !CanUseMagnetic()) {
        EndMagPull(true);
        return;
    }

    m_modeTimer -= dt;
    ApplyMagnetToTarget(*m_proxy, target->GetPosition(), kMagnetForce, kMagnetMaxDist, -1.0f);
    if (m_modeTimer <= 0.0f)
        EndMagPull(true);
}

void EnemyAiScript::TickMagOrbit(GameObject* target, float dt) {
    if (!target || !target->IsActive() || !CanUseMagnetic()) {
        EndMagPull(true);
        return;
    }

    const XMFLOAT3 center = target->GetPosition();
    XMFLOAT3 pos = m_proxy->GetPosition();
    float dx = pos.x - center.x;
    float dz = pos.z - center.z;
    float radius = sqrtf(dx * dx + dz * dz);
    if (radius < kMinOrbitRadius || radius > kMagnetMaxDist) {
        EndMagPull(true);
        return;
    }

    if (!m_isOrbiting) {
        m_isOrbiting = true;
        m_orbitCenter = center;
        m_orbitRadius = radius;
        m_orbitAngle = atan2f(dz, dx);
        m_orbitAngularVelocity = static_cast<float>(m_orbitSpinSign) * RandRange(0.8f, 1.8f);
        XMFLOAT3 vel = m_proxy->GetRigidbody().GetVelocity();
        vel.x = 0.0f;
        vel.z = 0.0f;
        m_proxy->GetRigidbody().SetVelocity(vel);
    }

    m_orbitCenter = center;
    m_modeTimer -= dt;

    const float angularAcceleration = static_cast<float>(m_orbitSpinSign) *
        (kOrbitSpinPower / (std::max)(0.01f, m_orbitRadius));
    m_orbitAngularVelocity += angularAcceleration * dt;
    const float maxAngularSpeed = kOrbitMaxLaunchSpeed / (std::max)(0.01f, m_orbitRadius);
    m_orbitAngularVelocity = std::clamp(m_orbitAngularVelocity, -maxAngularSpeed, maxAngularSpeed);
    m_orbitAngle += m_orbitAngularVelocity * dt;

    pos.x = m_orbitCenter.x + m_orbitRadius * cosf(m_orbitAngle);
    pos.z = m_orbitCenter.z + m_orbitRadius * sinf(m_orbitAngle);
    m_proxy->SetPosition(pos);
    m_proxy->GetRigidbody().SetVelocity({0.0f, m_proxy->GetRigidbody().GetVelocity().y, 0.0f});

    if (m_modeTimer <= 0.0f) {
        const float tangentX = -sinf(m_orbitAngle);
        const float tangentZ = cosf(m_orbitAngle);
        float launchSpeed = m_orbitAngularVelocity * m_orbitRadius;
        launchSpeed = std::clamp(launchSpeed, -kOrbitMaxLaunchSpeed, kOrbitMaxLaunchSpeed);
        XMFLOAT3 vel = m_proxy->GetRigidbody().GetVelocity();
        vel.x = tangentX * launchSpeed;
        vel.z = tangentZ * launchSpeed;
        m_proxy->GetRigidbody().SetVelocity(vel);
        OnOrbitLaunchEnd();
    }
}

void EnemyAiScript::TickChargeKnockback(GameObject* target, float dt) {
    if (!target || !target->IsActive()) {
        m_mode = EnemyDriveMode::Cooldown;
        m_modeTimer = RandRange(0.35f, 0.8f);
        return;
    }

    m_modeTimer -= dt;

    XMFLOAT3 dirToTarget = {
        target->GetPosition().x - m_proxy->GetPosition().x,
        0.0f,
        target->GetPosition().z - m_proxy->GetPosition().z
    };
    const float lenSq = dirToTarget.x * dirToTarget.x + dirToTarget.z * dirToTarget.z;
    if (lenSq > 0.0001f) {
        const float inv = 1.0f / sqrtf(lenSq);
        dirToTarget.x *= inv;
        dirToTarget.z *= inv;
    }

    if (IsTouchingTarget(target)) {
        TryApplyMagPullPlayerKnockback(target);
        return;
    }

    if (m_modeTimer <= 0.0f) {
        m_mode = EnemyDriveMode::Cooldown;
        m_modeTimer = RandRange(0.35f, 0.8f);
        m_currentIntent = EnemyIntent::Wander;
    }
}

void EnemyAiScript::TryPickupChaseItem() {
    if (!m_itemChaseTarget || !m_proxy) return;
    if (!m_itemChaseTarget->IsActive() || !ItemWorld_IsWorldPickup(*m_itemChaseTarget)) {
        m_itemChaseTarget = nullptr;
        if (m_mode == EnemyDriveMode::ChaseItem)
            m_mode = EnemyDriveMode::Wander;
        return;
    }

    const float distSq = DistSqXZ(m_proxy->GetPosition(), m_itemChaseTarget->GetPosition());
    if (distSq > kItemPickupScanRadius * kItemPickupScanRadius)
        return;

    if (ItemWorld_TryPickup(*m_itemChaseTarget, *m_proxy)) {
        m_itemChaseTarget = nullptr;
        m_itemPlaceCooldown = 0.0f;
        int targetScoreIndex = -1;
        GameObject* body = SelectAttackTarget(&targetScoreIndex);
        if (!body)
            body = FallbackOpponentPlayer(this);
        if (body) {
            m_mode = EnemyDriveMode::ChasePlace;
            m_chaseTarget = body->GetPosition();
            m_magnetTarget = body;
            m_magnetTargetScoreIndex = targetScoreIndex;
            m_modeTimer = 60.0f;
        } else {
            m_mode = EnemyDriveMode::Wander;
            m_modeTimer = 0.0f;
        }
    }
}

bool EnemyAiScript::ShouldPlaceHeldItem(GameObject* player, float* outDistToPlayer) const {
    if (!m_proxy || !HasHeldItem()) {
        if (outDistToPlayer) *outDistToPlayer = -1.0f;
        return false;
    }

    if (!player) {
        if (outDistToPlayer) *outDistToPlayer = 0.0f;
        return true;
    }

    const float distSq = DistSqXZ(m_proxy->GetPosition(), player->GetPosition());
    if (outDistToPlayer)
        *outDistToPlayer = sqrtf(distSq);

    switch (m_heldItemKind) {
        case ItemWorldKind::ShockwavePole:
        case ItemWorldKind::SpeedReducePole:
            return distSq <= kItemPlaceMaxDistance * kItemPlaceMaxDistance * 2.25f;
        case ItemWorldKind::Mine:
        case ItemWorldKind::ArrowSticker:
            return distSq <= kItemPlaceMaxDistance * kItemPlaceMaxDistance * 4.0f;
        default:
            return distSq <= kItemPlaceMaxDistance * kItemPlaceMaxDistance;
    }
}

void EnemyAiScript::TickHeldItem(GameObject* player, float dt) {
    if (!m_proxy || !HasHeldItem()) {
        m_debugDistToPlayer = -1.0f;
        m_debugShouldPlace = false;
        return;
    }

    if (m_itemPlaceCooldown > 0.0f)
        m_itemPlaceCooldown -= dt;

    m_debugShouldPlace = ShouldPlaceHeldItem(player, &m_debugDistToPlayer);

    if (m_itemPlaceCooldown > 0.0f) return;

    GameObject* held = m_heldItem;
    if (!held || !held->IsActive()) {
        ClearHeldItem();
        return;
    }

    if (!m_debugShouldPlace) return;

    float placeYaw = m_yaw;
    if (player) {
        float dx = player->GetPosition().x - m_proxy->GetPosition().x;
        float dz = player->GetPosition().z - m_proxy->GetPosition().z;
        if (dx * dx + dz * dz > 0.01f)
            placeYaw = atan2f(dx, dz) * (180.0f / 3.14159265f);
    }

    if (ItemWorld_TryPlace(*held, *m_proxy, placeYaw)) {
        ClearHeldItem();
        m_itemPlaceCooldown = kItemPlaceCooldown;
        ExitMagneticModes();
        m_mode = EnemyDriveMode::Cooldown;
        m_modeTimer = RandRange(0.4f, 0.9f);
    }
}

// ---- Debug UI ---------------------------------------------------------------

const char* EnemyAiScript::GetThoughtLine() const {
    if (m_isRespawning) return "respawn hold";
    if (m_reactionTimer > 0.0f) return "thinking...";
    if (m_controlState == EnemyControlState::Knockback) {
        if (m_stunImmobilize) return "stunned (immobile)";
        return "knocked back";
    }
    if (m_comboStep == 1) return "combo -> pull follow-up";
    switch (m_mode) {
        case EnemyDriveMode::ChaseBonus:
            return "chase bonus point";
        case EnemyDriveMode::ChaseItem:
            return m_itemChaseTarget ? "chase world pickup" : "chase item (lost target)";
        case EnemyDriveMode::ChasePlace:
            return m_debugShouldPlace ? "chase player -> place item" : "chase player (close to place)";
        case EnemyDriveMode::MagAttach:
            return m_magPurpose == EnemyMagPurpose::Utility
                ? "utility pull"
                : "rush pull -> power KB on hit";
        case EnemyDriveMode::MagPush:
            return "push self away (retreat)";
        case EnemyDriveMode::MagOrbit:
            return m_magPurpose == EnemyMagPurpose::Utility
                ? "utility orbit travel"
                : "combat orbit setup";
        case EnemyDriveMode::ChargeKnockback:
            return "charge rush -> knockback";
        case EnemyDriveMode::Cooldown:
            return HasHeldItem() ? "cooldown, ready to place item" : "cooldown after combo";
        case EnemyDriveMode::Wander:
        default:
            if (HasHeldItem()) return "wander (holding, need ChasePlace)";
            if (m_moveX == 0.0f && m_moveZ == 0.0f) return "wander / blunder pause";
            if (IsNearArenaEdge()) return "wander (steer from edge)";
            return "wander";
    }
}

void EnemyAiScript::SyncDebugPanel(float dt) {
    if (!g_activeEnemyScripts.empty() && g_activeEnemyScripts.front() != this)
        return;

    if (!AnyEnemyWantsDebugUi()) {
        if (g_enemyAiDebugText)
            ScriptApi_UIElementSetVisible(*g_enemyAiDebugText, false);
        return;
    }

    EnsureEnemyAiDebugText();
    if (!g_enemyAiDebugText) return;

    if (!EngineGUI_IsPlaying()) {
        ScriptApi_UIElementSetVisible(*g_enemyAiDebugText, false);
        return;
    }

    ScriptApi_UIElementSetVisible(*g_enemyAiDebugText, true);
    m_debugPanelTimer -= dt;
    if (m_debugPanelTimer > 0.0f)
        return;
    m_debugPanelTimer = kDebugPanelRefreshInterval;

    std::string text = "=== Enemy AI ===\n";
    int index = 0;
    for (EnemyAiScript* ai : g_activeEnemyScripts) {
        if (!ai || !ai->showAiDebug || !ai->m_proxy) continue;

        ++index;
        if (index > 1)
            text += "\n";

        const char* hostName = "Enemy";
        if (ai->m_hostObject && !ai->m_hostObject->GetName().empty())
            hostName = ai->m_hostObject->GetName().c_str();

        GameObject* player = PlayerScript_GetBody();
        const bool aimedCharge = player && ai->IsAimedAtPlayerForCharge(player);

        const bool held = ai->HasHeldItem();
        const float distPlayer = held ? ai->m_debugDistToPlayer : -1.0f;

        char block[1600];
        snprintf(block, sizeof(block),
            "[%d] %s  personality:%s (%d)\n"
            "  tuning:  bonus=%.2f item=%.2f combat=%.2f util=%.2f travel=%.0f mid=%.2f\n"
            "  control: %s\n"
            "  mode:    %s\n"
            "  intent:  %s  pending:%s\n"
            "  think:   %s\n"
            "  move:    (%.2f, %.2f)  grounded:%s  nearEdge:%s\n"
            "  timers:  mode=%.1fs  rethink=%.1fs  react=%.1fs  commit=%.1fs\n"
            "  item:    held=%s  chasing=%s  placeCd=%.1fs\n"
            "  place:   dist=%.1fm  inRange=%s\n"
            "  cd:      magC=%.1fs  magU=%.1fs  charge=%.1fs\n"
            "  energy:  %.0f/%.0f  overheated=%s  mag:%s\n"
            "  combo:   step=%d  fatigue=%d  midTravel=%s\n"
            "  charge:  aimed=%s  orbiting=%s\n"
            "  scores:  travel=%.2f  pull=%.2f  orbit=%.2f",
            index,
            hostName,
            PersonalityName(ai->personality),
            ai->personality,
            ai->m_tuning.bonusWeight,
            ai->m_tuning.itemWeight,
            ai->m_tuning.combatWeight,
            ai->m_tuning.utilityMagnetWeight,
            ai->m_tuning.magTravelMinDist,
            ai->m_tuning.midChaseTravelChance,
            ControlStateName(ai->m_controlState),
            DriveModeName(ai->m_mode),
            IntentName(ai->m_currentIntent),
            IntentName(ai->m_pendingIntent),
            ai->GetThoughtLine(),
            ai->m_moveX,
            ai->m_moveZ,
            ai->m_isGrounded ? "yes" : "no",
            ai->IsNearArenaEdge() ? "yes" : "no",
            ai->m_modeTimer,
            ai->m_rethinkTimer,
            ai->m_reactionTimer,
            ai->m_intentCommitTimer,
            held ? ItemKindName(ai->m_heldItemKind) : "none",
            ai->m_itemChaseTarget ? "yes" : "no",
            ai->m_itemPlaceCooldown,
            distPlayer >= 0.0f ? distPlayer : -1.0f,
            held ? (ai->m_debugShouldPlace ? "yes" : "no") : "n/a",
            ai->m_magCombatCooldown,
            ai->m_magUtilityCooldown,
            ai->m_chargeCooldownRemaining,
            ai->m_magneticEnergy,
            kMagneticEnergyMax,
            ai->m_magneticOverheated ? "yes" : "no",
            MagPurposeName(ai->m_magPurpose),
            ai->m_comboStep,
            ai->m_recentCombatMagUses,
            ai->m_usedMidChaseTravel ? "used" : "no",
            aimedCharge ? "yes" : "no",
            ai->m_isOrbiting ? "yes" : "no",
            ai->m_debugIntentScores[static_cast<int>(EnemyIntent::MagTravel)],
            ai->m_debugIntentScores[static_cast<int>(EnemyIntent::MagAttackPull)],
            ai->m_debugIntentScores[static_cast<int>(EnemyIntent::MagAttackOrbit)]);
        text += block;
        if (index >= 4) {
            text += "\n  ...";
            break;
        }
    }

    if (index == 0)
        text += "(no active enemies)\n";

    ScriptApi_UITextSetText(*g_enemyAiDebugText, text.c_str());
}

// ---- Main update ------------------------------------------------------------

void EnemyAiScript::OnUpdate(engine::GameObject& self, float dt) {
    if (!m_proxy || m_registryIndex < 0) return;

    PlayerNametag_Draw(GetPlayerIndex(), m_proxy->m_position);

    if (GameMode_IsTraining()) {
        if (engine::Transform* t = ScriptApi_FindTransform(self)) {
            t->localPosition = m_proxy->m_position;
            t->localRotation.y = m_yaw;
        }
        CenterPole_UpdateDynamicPosition(m_registryIndex, m_proxy->m_position);
        SyncEnemyTeamMaterial(self, GetPlayerIndex());
        ClearEnemyEmissionGlow(self);
        SyncEnemyAnimation(self);
        return;
    }

    if (!RoundManager_IsGameplayActive()) {
        TickMagneticEnergy(dt);
        SyncMagneticEnergyUI();
        SyncEnemyAnimation(self);
        SyncDebugPanel(dt);
        return;
    }

    if (m_isRespawning) {
        TickMagneticEnergy(dt);
        SyncMagneticEnergyUI();
        SyncEnemyAnimation(self);
        SyncDebugPanel(dt);
        return;
    }

    GameObject* player = PlayerScript_GetBody();
    int selectedTargetScoreIndex = -1;
    GameObject* selectedTarget = SelectAttackTarget(&selectedTargetScoreIndex);
    if (!selectedTarget)
        selectedTarget = FallbackOpponentPlayer(this);

    MagnetLinkVfx::Instance().SetSettings(BuildMagnetHelixSettings());

    TickMagneticEnergy(dt);
    SyncMagneticEnergyUI();

    if (IsMagPullActive() && m_magnetTarget)
        TryFinishMagPullPlayerHit(m_magnetTarget);

    if (m_controlState == EnemyControlState::Knockback && m_stunImmobilize)
        HandlePlayerImpact(player, dt);

    if (m_controlState == EnemyControlState::Knockback) {
        TickKnockbackState(self, dt);
        SyncEnemyAnimation(self);
        SyncDebugPanel(dt);
        return;
    }

    if (m_magCombatCooldown > 0.0f)
        m_magCombatCooldown -= dt;
    if (m_magUtilityCooldown > 0.0f)
        m_magUtilityCooldown -= dt;
    if (m_chargeCooldownRemaining > 0.0f)
        m_chargeCooldownRemaining -= dt;

    TickArenaFootprint(dt);
    TickGroundStatus(dt);
    if (!IsMagPullActive())
        HandlePlayerImpact(player, dt);

    if (m_controlState == EnemyControlState::Knockback) {
        TickKnockbackState(self, dt);
        SyncEnemyAnimation(self);
        SyncDebugPanel(dt);
        return;
    }

    TickWanderDirection(dt);
    TickPathVariation(dt);
    TickHeldItem(selectedTarget, dt);
    CheckComboTransitions(m_comboTarget ? m_comboTarget : selectedTarget, dt);

    if (m_mode == EnemyDriveMode::ChasePlace && !HasHeldItem())
        m_mode = EnemyDriveMode::Wander;

    const bool chasingBonus = (m_mode == EnemyDriveMode::ChaseBonus && m_modeTimer > 0.0f);
    const bool chasingItem  = (m_mode == EnemyDriveMode::ChaseItem && m_modeTimer > 0.0f);
    const bool chasingPlace = (m_mode == EnemyDriveMode::ChasePlace && HasHeldItem());
    const bool chasing      = chasingBonus || chasingItem || chasingPlace;

    GameObject* magneticTarget = nullptr;
    if (m_mode == EnemyDriveMode::ChargeKnockback) {
        GameObject* chargeTarget = m_magnetTarget ? m_magnetTarget : selectedTarget;
        TickChargeKnockback(chargeTarget, dt);
        ComputeMoveForMode(chargeTarget);
        m_proxy->m_rigidbody.AddForce(
            {m_moveX * kMoveSpeed * kChargeRushSpeedScale, 0.0f, m_moveZ * kMoveSpeed * kChargeRushSpeedScale},
            ForceMode::Acceleration);
    } else if (IsMagneticModeActive()) {
        magneticTarget = m_magnetTarget ? m_magnetTarget : selectedTarget;
        if (m_magnetTarget && (!m_magnetTarget->IsActive()))
            magneticTarget = nullptr;

        if (m_mode == EnemyDriveMode::MagAttach)
            TickMagPull(magneticTarget, dt);
        else if (m_mode == EnemyDriveMode::MagPush)
            TickMagPush(magneticTarget, dt);
        else if (m_mode == EnemyDriveMode::MagOrbit)
            TickMagOrbit(magneticTarget, dt);

        if (IsMagPullActive() && magneticTarget && !IsTouchingTarget(magneticTarget)) {
            ComputeMoveForMode(magneticTarget);
            const float pullSpeed = kMoveSpeed * 1.3f;
            m_proxy->m_rigidbody.AddForce(
                {m_moveX * pullSpeed, 0.0f, m_moveZ * pullSpeed},
                ForceMode::Acceleration);
        }
    } else {
        if (chasingItem && m_itemChaseTarget)
            m_chaseTarget = m_itemChaseTarget->GetPosition();
        if (chasingPlace && selectedTarget)
            m_chaseTarget = selectedTarget->GetPosition();

        if (chasing) {
            TryMidChaseMagTravel();
            m_modeTimer -= dt;
            if (chasingItem)
                TryPickupChaseItem();
            if (m_modeTimer <= 0.0f && !chasingPlace) {
                m_mode = EnemyDriveMode::Wander;
                m_itemChaseTarget = nullptr;
                m_usedMidChaseTravel = false;
            }
        }

        TickIntentSystem(player, dt);

        if (HasHeldItem() && selectedTarget && m_mode != EnemyDriveMode::ChasePlace &&
            m_mode != EnemyDriveMode::Cooldown && !IsMagPullActive() && m_reactionTimer <= 0.0f) {
            EnemyAiContext ctx = BuildContext(player);
            ApplyIntent(EnemyIntent::ChasePlace, ctx);
        }

        if (m_mode == EnemyDriveMode::Cooldown) {
            m_modeTimer -= dt;
            if (m_modeTimer <= 0.0f) {
                if (HasHeldItem() && selectedTarget) {
                    EnemyAiContext ctx = BuildContext(player);
                    ApplyIntent(EnemyIntent::ChasePlace, ctx);
                } else {
                    m_mode = EnemyDriveMode::Wander;
                    m_currentIntent = EnemyIntent::Wander;
                }
            }
        }

        ComputeMoveForMode(player);
        m_proxy->m_rigidbody.AddForce(
            {m_moveX * kMoveSpeed, 0.0f, m_moveZ * kMoveSpeed},
            ForceMode::Acceleration);
    }

    SyncMagnetLinkVfx(magneticTarget);

    // Spawn magnet helix VFX using the link state set by ShowMagnetLink() this frame.
    MagnetFieldSystem::Instance().Tick(dt);
    MagnetFieldSystem::Instance().HideMagnetLink();

    if (engine::Transform* t = ScriptApi_FindTransform(self)) {
        t->localPosition = m_proxy->m_position;
        t->localRotation.y = m_yaw;
    }
    CenterPole_UpdateDynamicPosition(m_registryIndex, m_proxy->m_position);

    SyncEnemyTeamMaterial(self, GetPlayerIndex());
    ClearEnemyEmissionGlow(self);

    SyncEnemyAnimation(self);
    SyncDebugPanel(dt);

    if (m_justExitedKnockback)
        m_justExitedKnockback = false;
}
