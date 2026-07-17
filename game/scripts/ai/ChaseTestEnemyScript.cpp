#include "ChaseTestEnemyScript.h"

#include "EnemyAiScript.h"
#include "PlayerScript.h"
#include "CenterPoleScript.h"
#include "engine/physics/core/rigidbody.h"
#include "engine/core/game_object.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/visual_component.h"
#include "engine/physics/collider/collider.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "scene_manager.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace DirectX;

REGISTER_SCRIPT(ChaseTestEnemyScript)

namespace {
    std::vector<ChaseTestEnemyScript*> g_chaseTestEnemies;

    class ChaseEnemyPhysicsProxy : public GameObject {
    public:
        engine::Transform* m_syncTransform = nullptr;

        ~ChaseEnemyPhysicsProxy() override {
            ScriptApi_DestroyCollider(*this);
        }

        void LogicUpdate(double /*elapsed_time*/) override {}
    };
}

GameObject* ChaseTestEnemy_GetPhysicsProxy(const ChaseTestEnemyScript* enemy) {
    return enemy ? enemy->GetPhysicsProxy() : nullptr;
}

ChaseTestEnemyScript* ChaseTestEnemy_FindByPhysicsProxy(GameObject* proxy) {
    if (!proxy) return nullptr;
    for (ChaseTestEnemyScript* enemy : g_chaseTestEnemies) {
        if (enemy && enemy->GetPhysicsProxy() == proxy)
            return enemy;
    }
    return nullptr;
}

const std::vector<ChaseTestEnemyScript*>& ChaseTestEnemy_GetActiveScripts() {
    return g_chaseTestEnemies;
}

void ChaseTestEnemy_ApplyKnockbackToProxy(GameObject* proxy,
                                          const XMFLOAT3& velocityChange,
                                          float knockbackDurationSeconds,
                                          float integrationDt) {
    ChaseTestEnemyScript* chase = ChaseTestEnemy_FindByPhysicsProxy(proxy);
    if (!chase) return;
    chase->ApplyKnockbackImpulse(velocityChange, knockbackDurationSeconds, integrationDt);
}

bool ChaseTestEnemy_IsAnyoneInRadius(const XMFLOAT3& center, float radius) {
    const float radiusSq = radius * radius;
    for (ChaseTestEnemyScript* enemy : g_chaseTestEnemies) {
        if (!enemy || !enemy->m_proxy || !enemy->m_proxy->IsActive()) continue;
        const XMFLOAT3 pos = enemy->m_proxy->GetPosition();
        const float dx = pos.x - center.x;
        const float dz = pos.z - center.z;
        if ((dx * dx + dz * dz) <= radiusSq)
            return true;
    }
    return false;
}

void ChaseTestEnemy_ApplyStunInRadius(const XMFLOAT3& center,
                                      float radius,
                                      float stunDurationSeconds,
                                      bool immobilize) {
    const float radiusSq = radius * radius;
    for (ChaseTestEnemyScript* enemy : g_chaseTestEnemies) {
        if (!enemy || !enemy->m_proxy || !enemy->m_proxy->IsActive()) continue;
        const XMFLOAT3 pos = enemy->m_proxy->GetPosition();
        const float dx = pos.x - center.x;
        const float dz = pos.z - center.z;
        if ((dx * dx + dz * dz) > radiusSq) continue;
        enemy->BeginStun(stunDurationSeconds);
        (void)immobilize;
    }
}

void ChaseTestEnemy_ApplyKnockbackInRadius(const XMFLOAT3& center,
                                           float radius,
                                           float baseImpulse,
                                           float lift,
                                           float knockbackDurationSeconds,
                                           float integrationDt,
                                           GameObject* placerBody,
                                           float placerImpulseScale) {
    const float radiusSq = radius * radius;
    const float placerScale = (std::max)(0.0f, placerImpulseScale);
    for (ChaseTestEnemyScript* enemy : g_chaseTestEnemies) {
        if (!enemy || !enemy->m_proxy || !enemy->m_proxy->IsActive()) continue;

        XMFLOAT3 delta = {
            enemy->m_proxy->GetPosition().x - center.x,
            0.0f,
            enemy->m_proxy->GetPosition().z - center.z
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
        const float impulseScale = (placerBody && enemy->m_proxy == placerBody) ? placerScale : 1.0f;
        const float impulse = baseImpulse * falloff * impulseScale;
        const float scaledLift = lift * falloff * impulseScale;
        const float useDt = (integrationDt > 0.0f) ? integrationDt : enemy->m_lastDt;

        enemy->ApplyKnockbackImpulse(
            {dir.x * impulse, scaledLift, dir.z * impulse},
            knockbackDurationSeconds,
            useDt);
    }
}

void ChaseTestEnemyScript::EndStunVisual(engine::GameObject& self) {
    m_stunSpinAngle = 0.0f;
    if (auto* vc = ScriptApi_FindVisualComponent(self))
        vc->emissionGlow = {0.0f, 0.0f, 0.0f, 0.0f};
    if (engine::Transform* t = ScriptApi_FindTransform(self)) {
        t->localPosition = m_proxy->m_position;
        t->localRotation.y = m_yaw;
    }
}

void ChaseTestEnemyScript::SyncProxyToHostTransform(engine::GameObject& self) {
    if (!m_proxy) return;
    CenterPole_UpdateDynamicPosition(m_registryIndex, m_proxy->m_position);
    if (engine::Transform* t = ScriptApi_FindTransform(self)) {
        t->localPosition = m_proxy->m_position;
        t->localRotation.y = m_yaw;
    }
}

void ChaseTestEnemyScript::ApplyKnockbackImpulse(const XMFLOAT3& velocityChange,
                                                 float durationSeconds,
                                                 float integrationDt) {
    const float duration = (durationSeconds > 0.0f)
        ? durationSeconds
        : (std::max)(0.05f, knockbackDuration);

    if (m_controlState == ControlState::Stunned && m_hostObject)
        EndStunVisual(*m_hostObject);

    m_controlState = ControlState::Knockback;
    m_knockbackTimer = duration;
    m_stunTimer = 0.0f;

    if (!m_proxy) return;

    Rigidbody& rb = m_proxy->GetRigidbody();
    XMFLOAT3 vel = rb.GetVelocity();
    vel.x += velocityChange.x;
    vel.y += velocityChange.y;
    vel.z += velocityChange.z;
    rb.SetVelocity(vel);

    // Physics runs before scripts; nudge position so knockback is visible this frame.
    const float stepDt = (std::max)(0.0f, integrationDt);
    if (stepDt > 0.0f) {
        XMFLOAT3 pos = m_proxy->GetPosition();
        pos.x += vel.x * stepDt;
        pos.y += vel.y * stepDt;
        pos.z += vel.z * stepDt;
        m_proxy->SetPosition(pos);
        if (m_hostObject)
            SyncProxyToHostTransform(*m_hostObject);
    }
}

void ChaseTestEnemyScript::BeginKnockback(float durationSeconds) {
    ApplyKnockbackImpulse({0.0f, 0.0f, 0.0f}, durationSeconds, 0.0f);
}

XMFLOAT3 ChaseTestEnemyScript::DirectionXZ(const XMFLOAT3& from, const XMFLOAT3& to) {
    XMFLOAT3 dir = {to.x - from.x, 0.0f, to.z - from.z};
    const float distSq = dir.x * dir.x + dir.z * dir.z;
    if (distSq > 0.001f) {
        const float inv = 1.0f / sqrtf(distSq);
        dir.x *= inv;
        dir.z *= inv;
    } else {
        dir = {1.0f, 0.0f, 0.0f};
    }
    return dir;
}

void ChaseTestEnemyScript::EnterKnockbackFromCollision(engine::GameObject& self) {
    if (m_controlState == ControlState::Stunned)
        EndStunVisual(self);
    m_controlState = ControlState::Knockback;
    m_knockbackTimer = (std::max)(0.05f, knockbackDuration);
    m_stunTimer = 0.0f;
}

void ChaseTestEnemyScript::ResolveCollisionWithBody(engine::GameObject& self, GameObject& other) {
    if (!m_proxy || !other.IsActive()) return;

    if (EnemyAiScript* ai = EnemyAi_FindByPhysicsProxy(&other)) {
        const XMFLOAT3 dirFromAiToOther = DirectionXZ(other.GetPosition(), m_proxy->GetPosition());
        if (m_hostObject)
            EnterKnockbackFromCollision(*m_hostObject);
        EnemyAi_OnProxyCollision(ai, dirFromAiToOther, *m_proxy);
        return;
    }

    for (ChaseTestEnemyScript* chase : g_chaseTestEnemies) {
        if (!chase || chase == this) continue;
        if (chase->m_proxy != &other && chase->m_hostObject != &other) continue;

        const XMFLOAT3 dirFromMeToOther = DirectionXZ(m_proxy->GetPosition(), other.GetPosition());
        m_proxy->GetRigidbody().AddForce(
            {-dirFromMeToOther.x * enemyImpactForce, 0.0f, -dirFromMeToOther.z * enemyImpactForce},
            ForceMode::Impulse);
        other.GetRigidbody().AddForce(
            {dirFromMeToOther.x * enemyImpactForce, 0.0f, dirFromMeToOther.z * enemyImpactForce},
            ForceMode::Impulse);
        if (m_hostObject)
            EnterKnockbackFromCollision(*m_hostObject);
        if (chase->m_hostObject)
            chase->EnterKnockbackFromCollision(*chase->m_hostObject);
        return;
    }

    const XMFLOAT3 dirFromMeToOther = DirectionXZ(m_proxy->GetPosition(), other.GetPosition());
    m_proxy->GetRigidbody().AddForce(
        {-dirFromMeToOther.x * enemyImpactForce, 0.0f, -dirFromMeToOther.z * enemyImpactForce},
        ForceMode::Impulse);
    other.GetRigidbody().AddForce(
        {dirFromMeToOther.x * enemyImpactForce, 0.0f, dirFromMeToOther.z * enemyImpactForce},
        ForceMode::Impulse);
    if (m_hostObject)
        EnterKnockbackFromCollision(*m_hostObject);
}

void ChaseTestEnemyScript::HandlePlayerImpact(GameObject* player) {
    if (!m_proxy || !player || m_controlState == ControlState::Knockback) return;

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

    const XMFLOAT3 dirFromMeToPlayer = DirectionXZ(m_proxy->GetPosition(), player->GetPosition());

    player->GetRigidbody().AddForce(
        {dirFromMeToPlayer.x * playerImpactForce, playerImpactLift, dirFromMeToPlayer.z * playerImpactForce},
        ForceMode::Impulse);
    m_proxy->GetRigidbody().AddForce(
        {-dirFromMeToPlayer.x * enemyImpactForce, 0.0f, -dirFromMeToPlayer.z * enemyImpactForce},
        ForceMode::Impulse);

    if (m_hostObject)
        EnterKnockbackFromCollision(*m_hostObject);
}

void ChaseTestEnemyScript::HandleEnemyImpacts(engine::GameObject& self) {
    if (!m_proxy || m_controlState == ControlState::Knockback) return;

    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (!scene) return;

    Collider* myCol = m_proxy->GetCollider();
    if (!myCol) {
        m_enemyContactSet.clear();
        return;
    }

    std::unordered_set<GameObject*> currentContacts;
    for (GameObject* other : scene->GetObjects()) {
        if (!other || !other->IsActive()) continue;
        if (other == m_proxy || other == m_hostObject) continue;
        if (other->GetTag() != Tag::TAG_ENEMY) continue;

        Collider* otherCol = other->GetCollider();
        if (!otherCol || !myCol->CheckCollision(otherCol)) continue;

        currentContacts.insert(other);
        if (m_enemyContactSet.count(other)) continue;

        ResolveCollisionWithBody(self, *other);
    }

    m_enemyContactSet.swap(currentContacts);
}

void ChaseTestEnemyScript::TickKnockbackState(engine::GameObject& self, float dt) {
    m_knockbackTimer -= dt;
    if (m_knockbackTimer <= 0.0f) {
        m_controlState = ControlState::Normal;
        m_knockbackTimer = 0.0f;
    }

    SyncProxyToHostTransform(self);
}

void ChaseTestEnemyScript::BeginStun(float durationSeconds) {
    const float duration = (durationSeconds > 0.0f)
        ? durationSeconds
        : (std::max)(0.05f, knockbackDuration);
    m_controlState = ControlState::Stunned;
    m_stunTimer = duration;
    m_knockbackTimer = 0.0f;
    m_stunSpinAngle = 0.0f;
    m_playerContactSet.clear();
    m_enemyContactSet.clear();
    if (m_proxy) {
        const XMFLOAT3 zero = {0.0f, 0.0f, 0.0f};
        m_proxy->GetRigidbody().SetVelocity(zero);
    }
}

void ChaseTestEnemyScript::SyncStunVisualRotation(engine::GameObject& self) {
    const float spinY = m_yaw + m_stunSpinAngle;
    if (engine::Transform* t = ScriptApi_FindTransform(self)) {
        t->localPosition = m_proxy->m_position;
        t->localRotation.y = spinY;
    }
}

void ChaseTestEnemyScript::TickStunState(engine::GameObject& self, float dt) {
    if (m_proxy)
        m_proxy->GetRigidbody().SetVelocity({0.0f, 0.0f, 0.0f});

    const float spinSpeed = (std::max)(0.0f, stunSpinSpeed);
    m_stunSpinAngle += spinSpeed * dt;
    if (m_stunSpinAngle >= 360.0f)
        m_stunSpinAngle = fmodf(m_stunSpinAngle, 360.0f);

    if (m_proxy) {
        const XMFLOAT3 pos = m_proxy->GetPosition();
        ScriptApi_BillboardAdd(
            {pos.x, pos.y + 1.1f, pos.z},
            {0.45f, 0.45f},
            {1.0f, 0.92f, 0.15f, 0.85f},
            nullptr,
            m_stunSpinAngle);
    }

    if (auto* vc = ScriptApi_FindVisualComponent(self))
        vc->emissionGlow = {1.0f, 0.92f, 0.15f, 1.0f};

    SyncStunVisualRotation(self);

    m_stunTimer -= dt;
    if (m_stunTimer <= 0.0f) {
        m_controlState = ControlState::Normal;
        m_stunTimer = 0.0f;
        m_stunSpinAngle = 0.0f;
        if (auto* vc = ScriptApi_FindVisualComponent(self))
            vc->emissionGlow = {0.0f, 0.0f, 0.0f, 0.0f};
        if (engine::Transform* t = ScriptApi_FindTransform(self)) {
            t->localPosition = m_proxy->m_position;
            t->localRotation.y = m_yaw;
        }
    }

    CenterPole_UpdateDynamicPosition(m_registryIndex, m_proxy->m_position);
}

void ChaseTestEnemyScript::CheckGroundStatus() {
    m_isGrounded = false;
    if (!m_proxy || !m_proxy->GetCollider()) return;

    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (!scene) return;

    Collider* myCol = m_proxy->GetCollider();
    const XMFLOAT3 myPos = m_proxy->GetPosition();
    float myHalfHeight = 0.5f;
    if (myCol->GetType() == ColliderType::BOX)
        myHalfHeight = ScriptApi_GetBoxColliderSize(*static_cast<BoxCollider*>(myCol)).y * 0.5f;

    for (auto* other : scene->GetObjects()) {
        if (!other || other == m_proxy || !other->IsActive()) continue;
        Collider* otherCol = other->GetCollider();
        if (!otherCol || !myCol->CheckCollision(otherCol)) continue;

        const XMFLOAT3 otherPos = ScriptApi_GetColliderWorldCenter(*otherCol);
        if (myPos.y - otherPos.y > (myHalfHeight - 0.1f)) {
            m_isGrounded = true;
            break;
        }
    }
}

void ChaseTestEnemyScript::ChasePlayer(GameObject* player, float dt) {
    if (!m_proxy || !player) return;

    const XMFLOAT3 enemyPos = m_proxy->GetPosition();
    const XMFLOAT3 playerPos = player->GetPosition();
    float dx = playerPos.x - enemyPos.x;
    float dz = playerPos.z - enemyPos.z;
    const float distSq = dx * dx + dz * dz;
    if (distSq <= 0.01f) return;

    const float inv = 1.0f / sqrtf(distSq);
    dx *= inv;
    dz *= inv;

    m_yaw = atan2f(dx, dz) * (180.0f / 3.14159265f);

    const float speed = (std::max)(0.1f, moveSpeed);
    m_proxy->GetRigidbody().AddForce(
        {dx * speed, 0.0f, dz * speed},
        ForceMode::Acceleration);

    if (m_isGrounded) {
        XMFLOAT3 vel = m_proxy->GetRigidbody().GetVelocity();
        vel.y = (std::max)(vel.y, 0.0f);
        m_proxy->GetRigidbody().SetVelocity(vel);
    }

    (void)dt;
}

void ChaseTestEnemyScript::OnCreate(engine::GameObject& self) {
    m_hostObject = &self;
    m_proxy = new ChaseEnemyPhysicsProxy();
    m_proxy->m_position = {8.0f, 1.0f, 0.0f};
    m_proxy->m_scale    = {1.0f, 1.0f, 1.0f};

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

    if (engine::Transform* t = ScriptApi_FindTransform(self)) {
        m_proxy->SetPosition(t->localPosition);
        if (auto* chaseProxy = dynamic_cast<ChaseEnemyPhysicsProxy*>(m_proxy))
            chaseProxy->m_syncTransform = t;
        m_yaw = t->localRotation.y;
    }

    m_controlState = ControlState::Normal;
    m_stunTimer = 0.0f;
    m_playerContactSet.clear();
    m_enemyContactSet.clear();

    CenterPole_AddDynamicTarget(m_proxy->m_position, m_proxy, &m_registryIndex);
    g_chaseTestEnemies.push_back(this);
}

GameObject* ChaseTestEnemyScript::GetRuntimePhysicsProxy(const GameObject& /*self*/) const {
    return m_proxy;
}

void ChaseTestEnemyScript::OnDestroy(engine::GameObject& self) {
    (void)self;
    g_chaseTestEnemies.erase(
        std::remove(g_chaseTestEnemies.begin(), g_chaseTestEnemies.end(), this),
        g_chaseTestEnemies.end());
    CenterPole_RemoveDynamicTarget(m_registryIndex);
    if (m_proxy) {
        if (SceneBase* scene = ScriptApi_GetCurrentScene())
            scene->RemoveObject(m_proxy);
        delete m_proxy;
        m_proxy = nullptr;
    }
}

void ChaseTestEnemyScript::OnUpdate(engine::GameObject& self, float dt) {
    if (!m_proxy) return;

    m_lastDt = (std::max)(dt, 1.0f / 240.0f);
    GameObject* player = PlayerScript_GetBody();

    if (m_controlState == ControlState::Stunned) {
        HandlePlayerImpact(player);
        HandleEnemyImpacts(self);
        if (m_controlState == ControlState::Stunned)
            TickStunState(self, dt);
        else if (m_controlState == ControlState::Knockback)
            TickKnockbackState(self, dt);
        return;
    }

    if (m_controlState == ControlState::Knockback) {
        TickKnockbackState(self, dt);
        return;
    }

    CheckGroundStatus();
    ChasePlayer(player, dt);
    HandlePlayerImpact(player);
    HandleEnemyImpacts(self);
    SyncProxyToHostTransform(self);
}
