/*****************************************************
 * File: game/scripts/EnemyAiScript.h
 * Description: ServerAiDriver-style enemy AI for single-player.
 *              Physics proxy + DriveMode state machine (wander, bonus chase,
 *              magnet attach/orbit/push-retreat). Personality presets tune behavior.
 *****************************************************/
#pragma once
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"
#include "engine/scene/scene_node.h"
#include "../magnet/MagnetHelixPath.h"
#include <DirectXMath.h>
#include <ItemWorldApi.h>
#include <unordered_set>
#include <vector>

class EnemyPhysicsProxy;
class GameObject;
class EnemyAiScript;
class UISprite;

void EnemyAi_OnProxyCollision(EnemyAiScript* ai,
                              const DirectX::XMFLOAT3& dirFromAiToOther,
                              engine::GameObject& otherBody);

void EnemyAi_ApplyShockwaveImpulse(const DirectX::XMFLOAT3& center,
                                   float radius,
                                   float baseImpulse,
                                   float lift,
                                   float knockbackDurationSeconds = -1.0f,
                                   engine::GameObject* placerBody = nullptr,
                                   float placerImpulseScale = 1.0f);

void EnemyAi_ApplyStunInRadius(const DirectX::XMFLOAT3& center,
                               float radius,
                               float stunDurationSeconds,
                               bool immobilize = false);

bool EnemyAi_IsAnyoneInRadius(const DirectX::XMFLOAT3& center, float radius);

void EnemyAi_ApplyDirectionalImpulseToProxy(engine::GameObject* proxy,
                                            const DirectX::XMFLOAT3& direction,
                                            float impulse,
                                            float lift,
                                            float knockbackDurationSeconds = -1.0f);

EnemyAiScript* EnemyAi_FindByPhysicsProxy(engine::GameObject* proxy);

const std::vector<EnemyAiScript*>& EnemyAi_GetActiveScripts();
void EnemyAi_ResetSessionState();
bool EnemyAi_IsRespawning(const EnemyAiScript* ai);
bool EnemyAi_IsBodyRespawning(const engine::GameObject* body);
engine::GameObject* EnemyAi_GetPhysicsProxy(const EnemyAiScript* ai);
engine::GameObject* EnemyAi_GetHostObject(EnemyAiScript* ai);
void EnemyAi_BeginRespawnAt(EnemyAiScript* ai, const DirectX::XMFLOAT3& holdPosition);
void EnemyAi_TickRespawnHold(EnemyAiScript* ai);
void EnemyAi_EndRespawn(EnemyAiScript* ai);
bool EnemyAi_TryGetSpawnPosition(const EnemyAiScript* ai, DirectX::XMFLOAT3& outPosition);

enum class EnemyControlState {
    Normal,
    Knockback
};

enum class EnemyDriveMode {
    Wander,
    Cooldown,
    ChaseBonus,
    ChaseItem,
    ChasePlace,
    ChargeKnockback,
    MagAttach,
    MagOrbit,
    MagPush
};

enum class EnemyIntent {
    Wander,
    ChaseItem,
    ChaseBonus,
    ChasePlace,
    MagTravel,
    MagRetreat,
    MagAttackPull,
    MagAttackOrbit,
    ChargeAttack,
    Count
};

enum class EnemyMagPurpose {
    None,
    Utility,
    Combat
};

enum class EnemyPersonality : int {
    Balanced    = 0,
    PointHunter = 1,
    Aggressor   = 2,
    Collector   = 3,
    Trickster   = 4,
    Cautious    = 5,
    ExtremeAggressor = 6,
    Custom      = 99
};

struct EnemyAiTuning {
    float aggro                = 1.0f;
    float bonusWeight          = 0.42f;
    float itemWeight           = 0.42f;
    float combatWeight         = 0.55f;
    float utilityMagnetWeight  = 1.0f;
    float intentNoise          = 0.12f;
    float comboMultiplier      = 1.0f;
    float hesitationChance     = 0.10f;
    float wanderBaseDelay      = 0.28f;
    float wanderBlunderChance  = 0.14f;
    float itemChaseChance      = 0.42f;
    float playerAttackChance   = 1.0f;
    float chargeChance         = 0.38f;
    float reactionDelayMin     = 0.15f;
    float reactionDelayMax     = 0.40f;
    float magTravelMinDist     = 18.0f;
    float magUtilityEnergyMin  = 0.35f;
    float comboChainChance     = 0.45f;
    float midChaseTravelChance = 0.30f;
    float utilityCooldownScale = 2.0f;
};

struct EnemyAiContext {
    float distToGoal       = 0.0f;
    float distToTarget     = 0.0f;
    float energyRatio      = 1.0f;
    bool  leading          = false;
    bool  behind           = false;
    bool  nearEdge         = false;
    bool  poleAvailable    = false;
    bool  targetCharging   = false;
    bool  hasBonusZone     = false;
    bool  insideBonusZone  = false;
    bool  utilityCdClear   = true;
    bool  combatCdClear    = true;
    bool  chargeCdClear    = true;
    bool  canUseMagnetic   = true;
    bool  utilityEnergyOk  = true;
    bool  combatFatigued   = false;
    bool  justExitedKnockback = false;
    int   attackScoreIndex = -1;
    GameObject* attackTarget = nullptr;
    GameObject* pickup       = nullptr;
    GameObject* travelPole   = nullptr;
    DirectX::XMFLOAT3 goalPos     = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 bonusCenter   = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 arenaCenterDir = {0.0f, 0.0f, 0.0f};
};

class EnemyAiScript : public Script {
    friend void EnemyAi_ApplyShockwaveImpulse(const DirectX::XMFLOAT3& center,
                                               float radius,
                                               float baseImpulse,
                                               float lift,
                                               float knockbackDurationSeconds,
                                               engine::GameObject* placerBody,
                                               float placerImpulseScale);
    friend void EnemyAi_ApplyStunInRadius(const DirectX::XMFLOAT3& center,
                                          float radius,
                                          float stunDurationSeconds,
                                          bool immobilize);
    friend bool EnemyAi_IsAnyoneInRadius(const DirectX::XMFLOAT3& center, float radius);
    friend void EnemyAi_OnProxyCollision(EnemyAiScript* ai,
                                         const DirectX::XMFLOAT3& dirFromAiToOther,
                                         engine::GameObject& otherBody);
    friend EnemyAiScript* EnemyAi_FindByPhysicsProxy(engine::GameObject* proxy);
    friend void EnemyAi_ApplyDirectionalImpulseToProxy(engine::GameObject* proxy,
                                                       const DirectX::XMFLOAT3& direction,
                                                       float impulse,
                                                       float lift,
                                                       float knockbackDurationSeconds);
    friend class EnemyPhysicsProxy;

#define ENEMY_AI_PUBLIC_FIELDS(X) \
    X(int,   personality,        0) \
    X(int,   playerIndex,        1) \
    X(int,   teamId,             0) \
    X(bool,  showAdvancedTuning, false) \
    X(bool,  showAiDebug,        true) \
    X(std::string, runClip,      "run") \
    X(std::string, idleClip,     "idle")

#define ENEMY_AI_ADVANCED_FIELDS(X) \
    X(float, itemChaseChance,    0.42f) \
    X(float, playerAttackChance, 1.0f) \
    X(float, chargeChance,       0.38f) \
    X(float, helixAmpMin,        0.15f) \
    X(float, helixAmpMax,        0.6f) \
    X(float, helixTurnsMin,      2.0f) \
    X(float, helixTurnsMax,      6.5f) \
    X(float, helixRotateSpeed,   1.75f) \
    X(float, helixTravelTime,    1.2f) \
    X(float, helixParticleSize,  0.3f) \
    X(float, intentNoise,        0.12f) \
    X(float, reactionDelayMin,   0.15f) \
    X(float, reactionDelayMax,   0.40f) \
    X(float, magTravelMinDist,   18.0f) \
    X(float, magUtilityEnergyMin, 0.35f) \
    X(float, comboChainChance,   0.45f) \
    X(float, midChaseTravelChance, 0.30f) \
    X(float, utilityCooldownScale, 2.0f)

#define ENEMY_AI_SCRIPT_FIELDS(X) \
    ENEMY_AI_PUBLIC_FIELDS(X) \
    ENEMY_AI_ADVANCED_FIELDS(X)

public:
    ENEMY_AI_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

    void OnCreate (engine::GameObject& self) override;
    void OnUpdate (engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;

    GameObject* GetRuntimePhysicsProxy(const GameObject& self) const override;

    bool IsRespawning() const { return m_isRespawning; }
    int GetPlayerIndex() const;
    int GetTeamId() const { return teamId; }
    GameObject* GetPhysicsProxy() const;
    GameObject* GetHostObject() const { return m_hostObject; }
    void BeginRespawnAt(const DirectX::XMFLOAT3& holdPosition);
    void TickRespawnHold();
    void EndRespawn();
    void SyncRespawnVisual();
    const DirectX::XMFLOAT3& GetSpawnPosition() const { return m_spawnPosition; }

    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

    GameObject* GetHeldItemObject() const { return m_heldItem; }
    ItemWorldKind GetHeldItemKind() const { return m_heldItemKind; }
    bool HasHeldItem() const { return m_heldItem != nullptr; }
    void SetHeldItem(GameObject* item, ItemWorldKind kind);
    void ClearHeldItem();

private:
    void  ResolveTuning();
    void  ApplyPersonalityPreset(EnemyPersonality preset);
    void  OverlayAdvancedFieldsOntoTuning();
    EnemyAiTuning BuildTuningFromAdvancedFields() const;
    float ProfileAggro() const;
    void  TickWanderDirection(float dt);
    void  TickPathVariation(float dt);
    GameObject* SelectAttackTarget(int* outScoreIndex = nullptr) const;
    GameObject* SelectPoleAssistTarget(const DirectX::XMFLOAT3& goal) const;
    void  TryPickNewGoal(GameObject* player);
    EnemyAiContext BuildContext(GameObject* player) const;
    float ScoreIntent(EnemyIntent intent, const EnemyAiContext& ctx) const;
    EnemyIntent PickIntent(const EnemyAiContext& ctx);
    void  QueueIntent(EnemyIntent intent);
    void  ApplyIntent(EnemyIntent intent, const EnemyAiContext& ctx);
    void  TickIntentSystem(GameObject* player, float dt);
    void  TryMidChaseMagTravel();
    void  CheckComboTransitions(GameObject* target, float dt);
    void  OnOrbitLaunchEnd();
    void  RecordMagUse(bool combat);
    bool  IsIntentCommitted() const;
    bool  CanRethinkIntent() const;
    void  ComputeMoveForMode(GameObject* player);
    void  TickMagPull(GameObject* target, float dt);
    void  TickMagPush(GameObject* target, float dt);
    void  TickMagOrbit(GameObject* target, float dt);
    void  TickChargeKnockback(GameObject* target, float dt);
    bool  TickMagPullCollisionStop(GameObject* target, float dt);
    bool  IsTouchingTarget(GameObject* target) const;
    bool  TryFinishMagPullPlayerHit(GameObject* target);
    bool  TryApplyMagPullPlayerKnockback(GameObject* target);
    float ComputeMagPullTouchDist(GameObject* target) const;
    void  EndMagPull(bool cooldown);
    void  TickHeldItem(GameObject* player, float dt);
    bool  ShouldPlaceHeldItem(GameObject* player, float* outDistToPlayer = nullptr) const;
    void  TryPickupChaseItem();
    void  CheckGroundStatus();
    void  TickGroundStatus(float dt);
    void  HandlePlayerImpact(GameObject* player, float dt);
    void  UpdateArenaFootprint();
    void  TickArenaFootprint(float dt);
    void  SteerMoveTowardSafeXZ(float& moveX, float& moveZ) const;
    bool  IsNearArenaEdge() const;
    bool  IsDirectionTowardArenaCenter(float dirX, float dirZ) const;
    bool  IsAimedAtPlayerForCharge(GameObject* player) const;
    bool  MeetsChargeApproachSpeed(GameObject* player, const DirectX::XMFLOAT3& dirToPlayer) const;
    bool  IsMagneticModeActive() const;
    bool  IsMagPullActive() const;
    bool  IsTargetChargingToward(const GameObject* target) const;
    static const char* IntentName(EnemyIntent intent);
    static const char* MagPurposeName(EnemyMagPurpose purpose);
    static const char* PersonalityName(int personalityValue);
    float Rand01() const;
    void  BeginKnockback(float durationSeconds = -1.0f, bool immobilize = false);
    void  EndStunVisual(engine::GameObject& self);
    void  TickKnockbackState(engine::GameObject& self, float dt);
    void  SyncStunVisualRotation(engine::GameObject& self);
    void  ExitMagneticModes();
    void  SyncDebugPanel(float dt);
    const char* GetThoughtLine() const;
    void SetupVisualAnimations(engine::GameObject& self);
    bool IsEnemyLocomoting() const;
    void SyncEnemyAnimation(engine::GameObject& self);
    void BindMagneticEnergyUI();
    void TickMagneticEnergy(float dt);
    void SyncMagneticEnergyUI() const;
    bool CanUseMagnetic() const;
    bool IsMagneticEnergyDraining(GameObject* target) const;
    void EnterMagneticOverheat();
    MagnetHelixPath::MagnetHelixSettings BuildMagnetHelixSettings() const;
    void SyncMagnetLinkVfx(GameObject* target);

    EnemyPhysicsProxy* m_proxy         = nullptr;
    engine::GameObject* m_hostObject   = nullptr;
    int                m_registryIndex = -1;

    EnemyDriveMode m_mode = EnemyDriveMode::Wander;
    EnemyIntent    m_currentIntent = EnemyIntent::Wander;
    EnemyIntent    m_pendingIntent = EnemyIntent::Wander;
    EnemyMagPurpose m_magPurpose = EnemyMagPurpose::None;
    float m_modeTimer     = 0.0f;
    float m_rethinkTimer  = 0.0f;
    float m_reactionTimer = 0.0f;
    float m_intentCommitTimer = 0.0f;
    float m_magCombatCooldown = 0.0f;
    float m_magUtilityCooldown = 0.0f;
    float m_chargeCooldownRemaining = 0.0f;
    float m_magFatigueTimer = 0.0f;
    int   m_recentCombatMagUses = 0;
    int   m_comboStep = 0;
    float m_comboPullTimer = 0.0f;
    GameObject* m_comboTarget = nullptr;
    EnemyDriveMode m_postOrbitResumeMode = EnemyDriveMode::Wander;
    bool  m_postOrbitResumeActive = false;
    bool  m_usedMidChaseTravel = false;
    bool  m_justExitedKnockback = false;
    EnemyAiTuning m_tuning{};
    float m_debugIntentScores[static_cast<int>(EnemyIntent::Count)] = {};

    static constexpr float kArenaEdgePadding = 1.5f;
    DirectX::XMFLOAT3 m_arenaCenter = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_arenaSize   = {20.0f, 1.0f, 20.0f};
    float m_arenaSafeRadius = 8.5f;
    bool  m_hasArenaFootprint = false;
    float m_arenaFootprintRefreshTimer = 0.0f;
    float m_groundCheckTimer = 0.0f;
    float m_debugPanelTimer = 0.0f;
    float m_magObstacleScanTimer = 0.0f;

    float m_wanderTimer = 0.0f;
    float m_wanderDirX  = 1.0f;
    float m_wanderDirZ  = 0.0f;
    float m_pathVariationTimer = 0.0f;
    float m_pathSide = 1.0f;
    float m_pathStrength = 0.0f;
    float m_moveX       = 0.0f;
    float m_moveZ       = 0.0f;

    float m_yaw = 0.0f;
    bool  m_isGrounded = false;

    bool  m_isOrbiting = false;
    float m_orbitAngle = 0.0f;
    float m_orbitRadius = 2.0f;
    float m_orbitAngularVelocity = 0.0f;
    int   m_orbitSpinSign = 1;
    DirectX::XMFLOAT3 m_orbitCenter = {0.0f, 0.0f, 0.0f};

    DirectX::XMFLOAT3 m_chaseTarget = {0.0f, 0.0f, 0.0f};
    GameObject* m_magnetTarget = nullptr;
    int m_magnetTargetScoreIndex = -1;

    GameObject* m_itemChaseTarget = nullptr;
    GameObject* m_heldItem        = nullptr;
    ItemWorldKind m_heldItemKind = ItemWorldKind::Unknown;
    float       m_itemPlaceCooldown = 0.0f;
    bool        m_magPullKnockbackApplied = false;
    float       m_debugDistToPlayer = -1.0f;
    bool        m_debugShouldPlace = false;

    DirectX::XMFLOAT3 m_spawnPosition = {0.0f, 2.0f, 0.0f};
    DirectX::XMFLOAT3 m_respawnHoldPosition = {0.0f, 5.0f, 0.0f};
    bool  m_isRespawning = false;
    bool  m_respawnGravityEnabled = true;

    EnemyControlState m_controlState = EnemyControlState::Normal;
    float m_knockbackTimer = 0.0f;
    bool  m_stunImmobilize = false;
    float m_stunSpinAngle = 0.0f;

    std::unordered_set<GameObject*> m_playerContactSet;

    UISprite* m_magneticEnergyBarSprite = nullptr;
    float m_magneticEnergy = 50.0f;
    bool m_magneticOverheated = false;
    static constexpr float kMagneticEnergyMax = 50.0f;
    static constexpr float kMagneticEnergyDrainPerSec = 30.0f;
    static constexpr float kMagneticEnergyRegenPerSec = 12.0f;
    static constexpr float kMagneticEnergyOverheatUnlockRatio = 0.30f;
};
