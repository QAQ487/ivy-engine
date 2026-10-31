/*****************************************************
 * File: engine/scripting/Script.h
 * Project: d3d11_engine
 * Author: Terry Yeung
 * Created: 2026-03-13
 * Description: Script.h ? Lightweight base class for user-authored game
 *              scripts.  Script authors inherit this class and implement
 *              OnCreate / OnUpdate / OnDestroy exactly as they would have
 *              written a standalone player or enemy class before prefabs were
 *              introduced.
 *
 * Lifecycle summary:
 *   OnCreate  ? called once when the owning entity is initialised (analogous
 *               to a constructor / Init function in the legacy style).
 *   OnUpdate  ? called every frame while the entity is active.
 *   OnDestroy ? called once when the entity is torn down (analogous to
 *               Uninit / destructor cleanup).
 *
 * Serialization:
 *   Declare serialized fields once in the header with SCRIPT_FIELDS macros
 *   (see script_fields.h).  Prefab JSON only needs "scriptClass"; defaults
 *   come from C++.  Optional "properties" overrides are merged on load.
 *
 * Registration:
 *   Use REGISTER_SCRIPT(MyScript) in the script's .cpp file (see
 *   ScriptRegistrar.h).  The macro self-registers the class with ScriptFactory
 *   at static-init time so prefabs can reference it by class name.
 *****************************************************/
#pragma once

#include "libraries/nlohmann/json.hpp"

#include "../scene/scene_node.h"

// Script ? base class for all user-authored game scripts.
class Script {
public:
    virtual ~Script() = default;

    // ── Lifecycle ──────────────────────────────────────────────────────────
    // Called once after the script is created and its properties have been
    // deserialized from the prefab.  Use this for one-time initialisation
    // (e.g. caching component pointers, spawning child objects).
    virtual void OnCreate(engine::GameObject& self) {}

    // Called every frame while the owning entity is active.
    // 'dt' is the elapsed time in seconds since the previous frame.
    virtual void OnUpdate(engine::GameObject& self, float dt) {}

    // Conservative default: unaudited scripts never run on workers.
    // Override to return true only after an explicit parallel-safety audit.
    virtual bool Update_IsParallelSafe() const { return false; }

    // Called once just before the script is destroyed (scene reset or entity
    // removal).  Use this to release runtime resources and undo OnCreate work.
    virtual void OnDestroy(engine::GameObject& self) {}

    // Trigger volume enter/exit (forwarded from GameObject when either collider
    // is marked IsTrigger). Defaults are no-ops.
    virtual void OnTriggerEnter(engine::GameObject& self, engine::GameObject* other) {
        (void)self;
        (void)other;
    }
    virtual void OnTriggerExit(engine::GameObject& self, engine::GameObject* other) {
        (void)self;
        (void)other;
    }

    // Optional physics body used while playing (e.g. a runtime collider proxy).
    // Editor selection overlay uses this for collider placement during play.
    virtual GameObject* GetRuntimePhysicsProxy(const GameObject& self) const {
        (void)self;
        return nullptr;
    }

    // ── Serialization ──────────────────────────────────────────────────────
    // Return a JSON object that captures the script's configurable state.
    // Only fields that should survive a save/load round-trip need to appear.
    // The default returns an empty object (no persisted fields).
    virtual nlohmann::json Serialize() const { return {}; }

    // Re-apply fields that were previously saved by Serialize().
    // 'j' is the JSON object containing the properties to restore.
    // Called during prefab load before OnCreate so the script's properties are
    // set before initialisation.
    virtual void Deserialize(const nlohmann::json& j) {}
};
