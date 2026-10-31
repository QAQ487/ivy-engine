/*****************************************************
 * File: component.h
 * Project: d3d11_engine
 * Author: Terry Yeung
 * -----------------------------------------------------------------------------
 * REFACTOR (2026-03-13)
 *   Forward declaration changed from engine::GameObject to ::GameObject,
 *   since engine::GameObject is now just an alias for ::GameObject.
 *   All other API is unchanged.
 *****************************************************/
#pragma once
#include <string>

// Forward-declare the unified ::GameObject to avoid circular includes.
class GameObject;

// Base interface for all components attached to a GameObject.
class Component {
public:
    // The scene-graph node that owns this component.
    // Set automatically by GameObject::AddComponent / AddComponentRaw.
    GameObject* owner = nullptr;

    virtual ~Component() = default;

    // Returns the serialization type name (e.g. "SceneTransform", "visual").
    virtual const char* GetTypeName() const = 0;

    // ── Lifecycle ──────────────────────────────────────────────────────────
    // OnAttach   - allocate resources, reset flags (runs once on attach).
    // OnDetach   - release all resources owned by this component.
    // OnReset    - restore runtime state to pre-play defaults WITHOUT releasing
    //             resources.  Called by GameObject::Reset() on Return-to-Start.
    // Update     - per-frame logic.
    virtual void OnAttach() {}
    virtual void OnDetach() {}
    virtual void Update(double /*elapsed_time*/) {}
    virtual void OnReset() {}

    // ── Parameter injection ────────────────────────────────────────────────
    // Called by ScriptComponent for each key/value in the prefab JSON "params".
    virtual void SetParam(const std::string& /*key*/,
                          const std::string& /*value*/) {}

    // ── Serialization ──────────────────────────────────────────────────────
    virtual std::string Serialize() const { return "{}"; }
    virtual void Deserialize(const std::string& /*json*/) {}
};
