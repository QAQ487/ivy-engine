/*****************************************************
 * File: game/scripts/PlayerScript.h
 * Project: d3d11_engine
 * Description: PlayerScript – movement, camera, magnetism, orbit.
 *              Collider comes from the GameObject's ColliderComponent (editor GUI).
 *              Visual from VisualComponent.
 *****************************************************/
#pragma once
#include "../magnet/MagnetHelixPath.h"
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"
#include "engine/rendering/particle/particle_system.h"
#include "engine/scene/scene_node.h"
#include "ItemWorldApi.h"
#include <DirectXMath.h>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

class UISprite;
class UIBar;
class GameObject;
struct MagnetFieldConfig;

// Returns the human player physics body (nullptr if not in play).
GameObject* PlayerScript_GetBody();

void PlayerScript_ResetSessionState();

bool PlayerScript_IsRespawning();
void PlayerScript_BeginRespawnAt(const DirectX::XMFLOAT3& holdPosition);
void PlayerScript_TickRespawnHold();
void PlayerScript_EndRespawn();
bool PlayerScript_TryGetSpawnPosition(DirectX::XMFLOAT3& outPosition);

// Puts the active player into knockback (no walk, jump, magnet, or orbit).
void PlayerScript_BeginKnockback(float durationSeconds = -1.0f);

// Immobilizing stun: zero velocity, spin visual, block all movement and magnet skills.
void PlayerScript_BeginStunForBody(GameObject* body, float durationSeconds = -1.0f);

// Multiplayer-ready: applies knockback stun when body matches a local PlayerScript owner.
void PlayerScript_BeginKnockbackForBody(GameObject* body, float durationSeconds = -1.0f);

// Scales WASD / stick movement only (1 = normal). Does not affect magnet pull, push, or orbit.
void PlayerScript_SetMoveSpeedScaleForBody(GameObject* body, float scale);

bool PlayerScript_IsKnockbackActiveForBody(GameObject* body);

// Immediate multiplier on current knockback velocity (e.g. 0.2 = 80% reduction).
void PlayerScript_ScaleKnockbackVelocityForBody(GameObject* body, float scale);

// While true, knockback state clamps upward motion and pulls the body toward the ground.
void PlayerScript_SetKnockbackGroundStickForBody(GameObject* body, bool enabled);

void PlayerScript_ApplyKnockbackImpulseForBody(GameObject* body,
                                               DirectX::XMFLOAT3 impulse,
                                               float stunDurationSeconds = -1.0f);

// Power charge knockback on the player body (VelocityChange, same feel as player charge hits).
void PlayerScript_ApplyChargeKnockbackToBody(GameObject* body,
                                             const DirectX::XMFLOAT3& dirFromAttackerToBody,
                                             float horizontalForce,
                                             float lift,
                                             float stunDurationSeconds = -1.0f);

// True while LMB (or RT) magnetic pull is active on a valid aim target (not orbit / stun).
bool PlayerScript_IsMagneticChargeActive();

// True if the player already applied a charge collision impulse to this body this frame.
bool PlayerScript_WasChargeImpactAppliedToBody(const GameObject* body);

enum class PlayerControlState {
    Normal,
    Knockback
};

class PlayerScript : public Script {
#define PLAYER_SCRIPT_FIELDS(X) \
    X(float, moveSpeed,         5.0f) \
    X(float, cameraSensitivity, 0.4f) \
    X(float, cameraMinPitch,  -35.0f) \
    X(float, cameraMaxPitch,    60.0f) \
    X(float, magnetMaxRange,    47.0f) \
    X(float, magnetAttractStrength, 100.0f) \
    X(float, magnetRepulseStrength, 80.0f) \
    X(float, magnetAttractMaxSpeed, 20.0f) \
    X(float, magnetRepulseMaxSpeed, 18.0f) \
    X(float, magnetAttractFalloffDistance, 0.5f) \
    X(float, magnetOrbitStartSpeed, 7.0f) \
    X(float, magnetOrbitAcceleration, 5.0f) \
    X(float, magnetOrbitMinRadius, 0.5f) \
    X(float, magnetOrbitMaxRadius, 80.0f) \
    X(float, magnetOrbitRadiusAdjustSpeed, 12.0f) \
    X(float, magnetOrbitRadialSpring, 35.0f) \
    X(float, magnetOrbitRadialDamping, 10.0f) \
    X(float, magnetOrbitTangentialDamping, 8.0f) \
    X(float, magnetEnergyMax, 10.0f) \
    X(float, magnetEnergyDrainPerSecond, 1.0f) \
    X(float, magnetEnergyRegenPerSecond, 2.0f) \
    X(float, magnetOverheatUnlockRatio, 0.30f) \
    X(float, helixAmpMin, 0.15f) \
    X(float, helixAmpMax, 0.6f) \
    X(float, helixTurnsMin, 2.0f) \
    X(float, helixTurnsMax, 6.5f) \
    X(float, helixRotateSpeed, 1.75f) \
    X(float, helixTravelTime, 1.2f) \
    X(float, helixParticleSize, 0.3f) \
    X(std::string, runClip,     "run") \
    X(std::string, idleClip,    "idle") \
    X(std::string, danceClip,  "dance")

public:
    PLAYER_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

    void OnCreate (engine::GameObject& self) override;
    void OnUpdate (engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;

    void BeginKnockback(float durationSeconds = -1.0f, bool immobilize = false);
    void BeginStun(float durationSeconds = -1.0f);
    void BeginRespawnAt(const DirectX::XMFLOAT3& holdPosition);
    void TickRespawnHold();
    void EndRespawn();
    bool IsRespawning() const { return m_isRespawning; }
    const DirectX::XMFLOAT3& GetSpawnPosition() const { return m_spawnPosition; }
    void EndStunVisual(engine::GameObject& self);
    bool IsImmobilizingStun() const { return m_controlState == PlayerControlState::Knockback && m_stunImmobilize; }
    void SetMoveSpeedScale(float scale);
    void ScaleKnockbackVelocity(float scale);
    void SetKnockbackGroundStick(bool enabled);
    bool IsKnockbackActive() const { return m_controlState == PlayerControlState::Knockback; }
    bool IsMagneticChargeActive() const;

    SCRIPT_SERIALIZE_METHODS(PLAYER_SCRIPT_FIELDS)

    // True while this enemy physics body is on post-hit magnet lock cooldown (reticle + LMB/RMB/orbit start).
    bool IsEnemyMagnetCooldownActive(const engine::GameObject* enemyBody) const;
    bool IsMagnetTargetLocked(const engine::GameObject* targetBody) const;

    GameObject* GetHeldItemObject() const { return m_heldItem; }
    ItemWorldKind GetHeldItemKind() const { return m_heldItemKind; }
    bool HasHeldItem() const { return m_heldItem != nullptr; }
    void SetHeldItem(GameObject* item, ItemWorldKind kind);
    void ClearHeldItem();

private:
    void CheckGroundStatus(engine::GameObject& obj);
    void HandleMovement   (engine::GameObject& obj, double dt);
    void HandleJump       (engine::GameObject& obj);
    void HandleAiming     (engine::GameObject& obj, float magnetMaxDist);
    void HandleMagneticAttraction(engine::GameObject& obj, float dt);
    void HandleMagneticRepulsion (engine::GameObject& obj, float dt);
    void HandlePoleOrbit  (engine::GameObject& obj, float dt);
    void HandleEnemyImpact(engine::GameObject& obj, float dt);
    void HandleAimedTargetChargeImpact(engine::GameObject& obj, float dt);
    bool MeetsChargeApproachSpeed(const engine::GameObject& obj, const engine::GameObject& other,
                                  const DirectX::XMFLOAT3& dirToOther) const;
    void ApplyImpactToBody(engine::GameObject& obj, engine::GameObject& other,
                           const DirectX::XMFLOAT3& dirFromPlayerToOther, float dt,
                           bool chargeHit, bool aimedAtTarget);
    void MarkChargeImpactApplied(engine::GameObject* body);
    void BindChargeKnockbackCooldownUI();
    void TickChargeKnockbackCooldown(float dt);
    void SyncChargeKnockbackCooldownUI();
    void BeginChargeKnockbackCooldown();
    void BindMagneticEnergyUI();
    void TickMagneticEnergy(float dt);
    void SyncMagneticEnergyUI();
    struct MagnetFieldConfig BuildMagnetFieldConfig() const;
    MagnetHelixPath::MagnetHelixSettings BuildMagnetHelixSettings() const;
    bool CanUseMagnetic() const;
    bool IsMagneticEnergyDraining() const;
    bool IsMagneticPullInputActive() const;
    bool IsMagneticPushInputActive() const;
    bool IsMagneticOrbitHeld() const;
    void EnterMagneticOverheat();
    void SyncTransformFromPhysics(engine::GameObject& self);
    bool IsAimedTargetLocked() const;
    void TickKnockbackState(engine::GameObject& self, float dt);
    void SyncStunVisualRotation(engine::GameObject& self);
    void CancelOrbit();
    engine::ParticleEmitterConfig BuildOrbitTrailEmitterConfig() const;
    void EnsureOrbitTrailEmitter(engine::GameObject& obj);
    void StopOrbitTrail(bool destroyImmediately = false);
    void SyncLockOnUI();
    void SetupVisualAnimations(engine::GameObject& self);
    bool IsLocomoting(const engine::GameObject& obj) const;
    void SyncPlayerAnimation(engine::GameObject& self, float dt);
    void SetFacingFromDirection(const DirectX::XMFLOAT3& dir);
    void SyncFacingFromVelocity(const engine::GameObject& obj);
    void UpdateFacingYaw(float dt);

    DirectX::XMFLOAT3 GetForward(const engine::GameObject& obj) const;
    DirectX::XMFLOAT3 GetRight  (const engine::GameObject& obj) const;

    PlayerControlState m_controlState = PlayerControlState::Normal;
    float m_knockbackTimer = 0.0f;
    float m_moveSpeedScale = 1.0f;
    bool  m_knockbackGroundStick = false;
    bool  m_stunImmobilize = false;
    float m_stunSpinAngle = 0.0f;

    float m_yaw = 0.0f;        // camera orbit yaw (degrees)
    float m_facingYaw = 0.0f;  // visual facing from movement direction (degrees)
    float m_targetFacingYaw = 0.0f;
    bool  m_facingFromAccelerationThisFrame = false;
    bool  m_isGrounded = false;
    bool  m_isOrbiting = false;
    // While true, pull/push hold freezes CenterPole aim (no retarget until release).
    bool  m_magnetAimFrozen = false;
    int   m_orbitPoleIndex = -1;
    std::uint32_t m_orbitTrailEmitterId = 0;
    float m_postOrbitShieldTimer = 0.0f;

    UISprite* m_crosshair = nullptr;
    UISprite* m_chargeKnockbackCooldownSprite = nullptr;
    float m_chargeKnockbackCooldownRemaining = 0.0f;
    static constexpr const char* kChargeKnockbackCooldownSpriteName = "Knockback cooldown";
    UIBar* m_magneticEnergyBar = nullptr;
    UISprite* m_hudEnergyBarSprite = nullptr;
    float m_magneticEnergy = 10.0f;
    bool m_magneticOverheated = false;
    static constexpr const char* kMagneticEnergyBarName = "energy bar";
    static constexpr const char* kHudEnergyBarSpriteName = "HudP1EnergyBar";
    GameObject* m_heldItem = nullptr;
    ItemWorldKind m_heldItemKind = ItemWorldKind::Unknown;
    DirectX::XMFLOAT3 m_spawnPosition = {0.0f, 1.0f, 0.0f};
    DirectX::XMFLOAT3 m_respawnHoldPosition = {0.0f, 5.0f, 0.0f};
    bool m_isRespawning = false;
    float m_danceTimer = 0.0f;
    // Remaining seconds per enemy body: while > 0, magnet pull/push/orbit cannot target that enemy.
    std::unordered_map<engine::GameObject*, float> m_enemyMagnetCooldown;
    std::unordered_set<engine::GameObject*> m_enemyContactSet;
    std::unordered_set<engine::GameObject*> m_otherPlayerContactSet;
    int m_poleRegistryIndex = -1;
};
