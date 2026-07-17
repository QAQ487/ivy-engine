/*****************************************************
 * File: game/scripts/DummyPlayerScript.cpp
 * Project: d3d11_engine
 * Description: DummyPlayerScript implementation.
 *              Creates a physics proxy with gravity and a component-authored collider, then
 *              registers the dummy's live position in the CenterPole target
 *              registry so the real player can push / pull / orbit it.
 *              The dummy wanders randomly at configurable moveSpeed.
 *****************************************************/
#include "DummyPlayerScript.h"
#include "CenterPoleScript.h"
#include "engine/core/game_object.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/visual_component.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "scene_manager.h"
#include <cstdio>
#include <DirectXMath.h>
#include <cmath>
#include <cstdlib>

using namespace DirectX;

REGISTER_SCRIPT(DummyPlayerScript)

// Duration of each wander direction segment (seconds).
static constexpr float kWanderInterval = 2.5f;

// ---- Internal physics proxy -------------------------------------------------
// Mirrors the PlayerPhysicsProxy pattern: a minimal legacy GameObject used
// purely for rigidbody / collider integration.  The visual comes from the
// prefab node's VisualComponent.
class DummyPhysicsProxy : public GameObject {
public:
    engine::Transform* m_syncTransform = nullptr;

    ~DummyPhysicsProxy() override {
        ScriptApi_DestroyCollider(*this);
    }

    void LogicUpdate(double /*elapsed_time*/) override {}

    void IntegratePhysics(double elapsed_time) override {
        GameObject::IntegratePhysics(elapsed_time);
        // Keep the prefab Transform (and its VisualComponent) in sync with
        // the physics-driven position.
        if (m_syncTransform) {
            m_syncTransform->localPosition = m_position;
        }
    }
};

// ---- Lifecycle --------------------------------------------------------------

void DummyPlayerScript::OnCreate(engine::GameObject& self) {
    m_proxy = new DummyPhysicsProxy();

    // Default transform (overridden below if a prefab Transform is present).
    m_proxy->m_position = {5.0f, 1.0f, 0.0f};
    m_proxy->m_scale    = {1.0f, 1.0f, 1.0f};

    // PhysicsSystem only integrates bodies that have a collider. Build it on the proxy
    // (same authored ColliderComponent data as the prefab). Do not copy ColliderComponent
    // by value here — the GameScripts DLL does not link collider_component.cpp.
    if (ColliderComponent* cc = ScriptApi_FindColliderComponent(self)) {
        GameObject* savedOwner = cc->owner;
        cc->owner = m_proxy;
        ScriptApi_ApplyColliderComponent(*cc);
        cc->owner = savedOwner;
        m_proxy->m_colliderOwnedByHost = true;
        m_proxy->m_debugCollider = cc->showDebugCollider;
    }
    else {
        ScriptApi_CreateBoxCollider(*m_proxy, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f});
        m_proxy->m_debugCollider = true;
    }
    // ColliderComponent::ApplyToOwner marks the body kinematic; the dummy must stay dynamic.
    m_proxy->m_rigidbody.SetIsKinematic(false);

    // Physics: affected by gravity and by the forces the player applies.
    m_proxy->m_rigidbody.SetUseGravity(true);
    m_proxy->m_rigidbody.SetDrag(1.0f);
    m_proxy->m_rigidbody.SetMass(1.0f);

    // Tag is authored on the prefab node in the editor; propagate to the physics proxy.
    m_proxy->SetTag(self.GetTag());

    // Remove the prefab node's runtime collider so we do not double-register overlaps
    // with the proxy (SceneRuntime already applied it at play start).
    ScriptApi_DestroyCollider(self);
    self.m_debugCollider = false;

    // Register in the legacy scene so the physics / collision systems see it.
    if (SceneBase* scene = ScriptApi_GetCurrentScene())
        scene->AddObject(m_proxy);

    // Override spawn position from the prefab Transform if present.
    if (engine::Transform* t = ScriptApi_FindTransform(self)) {
        m_proxy->SetPosition(t->localPosition);
        m_proxy->m_syncTransform = t;
    }
    // Avoid post-script swept collision treating spawn as a teleport from (0,0,0).
    m_proxy->m_previousPosition = m_proxy->m_position;

    // Store the spawn position for respawning after a fall
    m_spawnPosition = m_proxy->m_position;
    m_respawnTimer  = -1.0f;

    // Seed initial wander direction (along +X to start) and randomise first timer
    // so multiple dummies don't all change direction at the same time.
    m_wanderDirX  = 1.0f;
    m_wanderDirZ  = 0.0f;
    m_wanderTimer = kWanderInterval * (0.1f + 0.9f * (static_cast<float>(std::rand()) /
                                                        static_cast<float>(RAND_MAX)));

    // Register as a magnetic target so PlayerScript can aim at, push, pull,
    // and orbit this dummy just like a regular pole.
    CenterPole_AddDynamicTarget(m_proxy->m_position, m_proxy, &m_registryIndex);
}

// ---- Pick a new random wander direction -------------------------------------
static void PickNewWanderDir(float& outX, float& outZ) {
    // Random angle in [0, 2π)
    float angle = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)
                  * 6.2831853f; // 2π
    outX = cosf(angle);
    outZ = sinf(angle);
}

void DummyPlayerScript::OnUpdate(engine::GameObject& self, float dt) {
    if (!m_proxy || m_registryIndex < 0) return;

    // ── Fall detection and respawn countdown ──────────────────────────────────
    if (m_proxy->m_position.y < 0.0f && m_respawnTimer < 0.0f) {
        m_respawnTimer = 5.0f;
    }

    if (m_respawnTimer >= 0.0f) {
        m_respawnTimer -= dt;

        if (m_respawnTimer <= 0.0f) {
            // Respawn at the original spawn position (silently, no UI)
            m_proxy->SetPosition(m_spawnPosition);
            DirectX::XMFLOAT3 zero = {0.0f, 0.0f, 0.0f};
            m_proxy->m_rigidbody.SetVelocity(zero);
            m_respawnTimer = -1.0f;
        }

        // Keep the registry position up-to-date even while falling/counting down
        CenterPole_UpdateDynamicPosition(m_registryIndex, m_proxy->m_position);
        if (engine::Transform* t = ScriptApi_FindTransform(self)) {
            t->localPosition = m_proxy->m_position;
        }
        return; // Skip wander AI while counting down
    }

    // ── Wander AI ───────────────────────────────────────────────────────────
    // Count down the direction timer; pick a new direction when it expires.
    m_wanderTimer -= dt;
    if (m_wanderTimer <= 0.0f) {
        PickNewWanderDir(m_wanderDirX, m_wanderDirZ);
        m_wanderTimer = kWanderInterval;
    }

    // Apply horizontal movement force unconditionally (same pattern as
    // PlayerScript::HandleMovement which does not guard on isGrounded).
    // Drag provides natural deceleration.
    m_proxy->m_rigidbody.AddForce(
        { m_wanderDirX * moveSpeed, 0.0f, m_wanderDirZ * moveSpeed },
        ForceMode::Acceleration
    );

    // ── Sync physics position → prefab Transform for rendering ─────────────
    // GameScene::Update only calls IntegratePhysics on TAG_PLAYER objects, so
    // DummyPhysicsProxy::IntegratePhysics (which writes m_position back to
    // m_syncTransform) is never called automatically.  We do it here instead
    // so DrawSceneModels() reads the correct, physics-updated position.
    if (engine::Transform* t = ScriptApi_FindTransform(self)) {
        t->localPosition = m_proxy->m_position;
    }

    // ── Keep registry position up-to-date ───────────────────────────────────
    // Push the live physics position into the aiming registry every frame so
    // magnetic forces are always computed against the dummy's current position.
    CenterPole_UpdateDynamicPosition(m_registryIndex, m_proxy->m_position);

    if (auto* vc = ScriptApi_FindVisualComponent(self))
        vc->emissionGlow = XMFLOAT4{0.0f, 0.0f, 0.0f, 0.0f};
}

GameObject* DummyPlayerScript::GetRuntimePhysicsProxy(const GameObject& /*self*/) const {
    return m_proxy;
}

void DummyPlayerScript::OnDestroy(engine::GameObject& self) {
    (void)self;

    // Unregister from the aiming registry before destroying the proxy so no
    // stale pointer is left in the registry.
    CenterPole_RemoveDynamicTarget(m_registryIndex);

    if (m_proxy) {
        if (SceneBase* scene = ScriptApi_GetCurrentScene())
            scene->RemoveObject(m_proxy);
        delete m_proxy;
        m_proxy = nullptr;
    }
}
