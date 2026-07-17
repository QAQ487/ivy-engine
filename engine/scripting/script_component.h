/*****************************************************
 * File: engine/components/script_component.h
 * Project: d3d11_engine
 * Author: Terry
 * Created: 2026-03-13
 * Description: ScriptComponent – a Component that wraps a Script subclass
 *              chosen by class name from prefab JSON.
 *
 * Prefab JSON:
 *   "Script": { "scriptClass": "EnemyScript" }
 *   Optional "properties" overrides header defaults when needed.
 *
 * Adding a new Script:
 *   1. Write a class that extends Script and place it in game/scripts/.
 *   2. In its .cpp file use REGISTER_SCRIPT(MyScript) (ScriptRegistrar.h).
 *   3. Reference the class name in the prefab JSON "scriptClass" field.
 *****************************************************/
#pragma once
#include "engine/ecs/component.h"
#include "engine/scripting/Script.h"
#include "engine/scripting/script_factory.h"
#include "libraries/nlohmann/json.hpp"
#include <memory>
#include <string>

// ScriptComponent wraps a Script (by scriptClass) at load time.
// Register component type "Script" with ComponentFactory so PrefabSerializer
// can instantiate it automatically.
class ScriptComponent : public Component {
public:
    // ── Component identity ────────────────────────────────────────────────
    const char* GetTypeName() const override { return "Script"; }

    // ── Lifecycle forwarding ──────────────────────────────────────────────
    void OnAttach() override;
    void OnDetach() override;
    void Update(double elapsed_time) override;
    void OnReset() override;
    void Teardown();

    // ── Serialization ─────────────────────────────────────────────────────
    std::string Serialize() const override;
    void Deserialize(const std::string& jsonStr) override;

    // ── Script accessor ───────────────────────────────────────────────────
    Script* GetScript() const { return m_script.get(); }
    std::string GetScriptClass() const { return m_scriptClass; }
    nlohmann::json GetEditableProperties() const;
    void ApplyProperties(const nlohmann::json& properties);

    void HotReload();

private:
    std::string             m_scriptClass;       // ScriptFactory lookup key
    nlohmann::json          m_scriptProperties;  // cached properties object
    ScriptFactory::ScriptPtr m_script;           // concrete Script instance
    bool                    m_scriptCreated = false;

    void CreateScript();
};

