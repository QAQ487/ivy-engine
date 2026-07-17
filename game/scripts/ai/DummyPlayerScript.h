/*****************************************************
 * File: game/scripts/DummyPlayerScript.h
 * Project: d3d11_engine
 * Description: DummyPlayerScript – a physics-driven stand-in player used to
 *              test the PlayerScript's push / pull / orbit mechanics on another
 *              "player" target instead of a static pole.
 *
 *              The dummy registers itself in the same CenterPole aiming
 *              registry as regular poles, so the PlayerScript's LMB pull,
 *              RMB push, and E-key orbit all work against it without any
 *              changes to PlayerScript.
 *
 *              The dummy wanders randomly on the XZ plane at configurable
 *              moveSpeed, picking a new random direction every few seconds.
 *              It has gravity and a collider from its Collider component so it responds physically to
 *              forces applied by the player.
 *
 * Prefab JSON:
 *   "Script": {
 *     "scriptClass": "DummyPlayerScript",
 *     "properties": {
 *       "moveSpeed": 5.0
 *     }
 *   }
 *****************************************************/
#pragma once
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"
#include "engine/scene/scene_node.h"
#include <DirectXMath.h>

// Forward declaration of the internal physics proxy (defined in DummyPlayerScript.cpp).
class DummyPhysicsProxy;

// DummyPlayerScript – wandering physics entity that acts as a magnetic target.
// The player can push, pull, and orbit around it just like a regular pole.
class DummyPlayerScript : public Script {
#define DUMMY_PLAYER_SCRIPT_FIELDS(X) \
    X(float, moveSpeed, 5.0f)

public:
    DUMMY_PLAYER_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

    // ── Lifecycle ─────────────────────────────────────────────────────────
    void OnCreate (engine::GameObject& self) override;
    void OnUpdate (engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;

    GameObject* GetRuntimePhysicsProxy(const GameObject& self) const override;

    SCRIPT_SERIALIZE_METHODS(DUMMY_PLAYER_SCRIPT_FIELDS)

private:
    DummyPhysicsProxy* m_proxy         = nullptr;
    int                m_registryIndex = -1; // index into the CenterPole target registry

    // Wander state
    float m_wanderTimer   = 0.0f; // seconds until next direction change
    float m_wanderDirX    = 1.0f; // current wander direction (normalised XZ)
    float m_wanderDirZ    = 0.0f;

    // Respawn state
    DirectX::XMFLOAT3 m_spawnPosition = {5.0f, 1.0f, 0.0f};
    float             m_respawnTimer  = -1.0f; // negative = not counting down
};
