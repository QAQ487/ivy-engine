#pragma once

#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"
#include <DirectXMath.h>
#include <unordered_set>
#include <vector>

class GameObject;
class ChaseTestEnemyScript;

void ChaseTestEnemy_ApplyStunInRadius(const DirectX::XMFLOAT3& center,
                                      float radius,
                                      float stunDurationSeconds,
                                      bool immobilize = true);

void ChaseTestEnemy_ApplyKnockbackInRadius(const DirectX::XMFLOAT3& center,
                                           float radius,
                                           float baseImpulse,
                                           float lift,
                                           float knockbackDurationSeconds = -1.0f,
                                           float integrationDt = 1.0f / 60.0f,
                                           engine::GameObject* placerBody = nullptr,
                                           float placerImpulseScale = 1.0f);

bool ChaseTestEnemy_IsAnyoneInRadius(const DirectX::XMFLOAT3& center, float radius);

ChaseTestEnemyScript* ChaseTestEnemy_FindByPhysicsProxy(engine::GameObject* proxy);

engine::GameObject* ChaseTestEnemy_GetPhysicsProxy(const ChaseTestEnemyScript* enemy);

const std::vector<ChaseTestEnemyScript*>& ChaseTestEnemy_GetActiveScripts();

void ChaseTestEnemy_ApplyKnockbackToProxy(engine::GameObject* proxy,
                                          const DirectX::XMFLOAT3& velocityChange,
                                          float knockbackDurationSeconds = -1.0f,
                                          float integrationDt = 1.0f / 60.0f);

class ChaseTestEnemyScript : public Script {
    friend void ChaseTestEnemy_ApplyKnockbackToProxy(engine::GameObject* proxy,
                                                     const DirectX::XMFLOAT3& velocityChange,
                                                     float knockbackDurationSeconds,
                                                     float integrationDt);
    friend void ChaseTestEnemy_ApplyStunInRadius(const DirectX::XMFLOAT3& center,
                                                 float radius,
                                                 float stunDurationSeconds,
                                                 bool immobilize);
    friend bool ChaseTestEnemy_IsAnyoneInRadius(const DirectX::XMFLOAT3& center, float radius);

#define CHASE_TEST_ENEMY_SCRIPT_FIELDS(X) \
    X(float, moveSpeed,         4.5f) \
    X(float, knockbackDuration, 0.45f) \
    X(float, stunSpinSpeed,    720.0f) \
    X(float, playerImpactForce, 6.0f) \
    X(float, playerImpactLift,  0.35f) \
    X(float, enemyImpactForce,  4.0f)

public:
    CHASE_TEST_ENEMY_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;

    GameObject* GetPhysicsProxy() const { return m_proxy; }
    GameObject* GetRuntimePhysicsProxy(const GameObject& self) const override;

    SCRIPT_SERIALIZE_METHODS(CHASE_TEST_ENEMY_SCRIPT_FIELDS)

    friend void ChaseTestEnemy_ApplyKnockbackInRadius(const DirectX::XMFLOAT3& center,
                                                    float radius,
                                                    float baseImpulse,
                                                    float lift,
                                                    float knockbackDurationSeconds,
                                                    float integrationDt,
                                                    engine::GameObject* placerBody,
                                                    float placerImpulseScale);

private:
    enum class ControlState {
        Normal,
        Stunned,
        Knockback
    };

    void BeginStun(float durationSeconds);
    void BeginKnockback(float durationSeconds);
    void ApplyKnockbackImpulse(const DirectX::XMFLOAT3& velocityChange,
                               float durationSeconds,
                               float integrationDt);
    void EndStunVisual(engine::GameObject& self);
    void EnterKnockbackFromCollision(engine::GameObject& self);
    void TickStunState(engine::GameObject& self, float dt);
    void TickKnockbackState(engine::GameObject& self, float dt);
    void SyncStunVisualRotation(engine::GameObject& self);
    void SyncProxyToHostTransform(engine::GameObject& self);
    void CheckGroundStatus();
    void ChasePlayer(GameObject* player, float dt);
    void HandlePlayerImpact(GameObject* player);
    void HandleEnemyImpacts(engine::GameObject& self);
    void ResolveCollisionWithBody(engine::GameObject& self, GameObject& other);
    static DirectX::XMFLOAT3 DirectionXZ(const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& to);

    GameObject* m_proxy = nullptr;
    int m_registryIndex = -1;

    ControlState m_controlState = ControlState::Normal;
    float m_stunTimer = 0.0f;
    float m_knockbackTimer = 0.0f;
    float m_stunSpinAngle = 0.0f;
    float m_lastDt = 1.0f / 60.0f;
    engine::GameObject* m_hostObject = nullptr;
    float m_yaw = 0.0f;
    bool m_isGrounded = false;
    std::unordered_set<engine::GameObject*> m_playerContactSet;
    std::unordered_set<engine::GameObject*> m_enemyContactSet;
};
