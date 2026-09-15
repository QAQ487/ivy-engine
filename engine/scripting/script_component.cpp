/*****************************************************
 * File: engine/components/script_component.cpp
 * Project: d3d11_engine
 * Author: Terry Yeung
 * Created: 2026-03-13
 * Description: ScriptComponent implementation.
 *****************************************************/
#include "script_component.h"
#include "../scripting/script_factory.h"
#include "engine/core/engine_gui.h"
#include <Windows.h>

// „Ÿ„Ÿ Lifecycle forwarding „Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ

void ScriptComponent::OnAttach() {
    // Do NOT call OnCreate here. Defer until the first Update() in play mode
    // to prevent game-side effects (enemy spawning, etc.) in editor mode.
}

void ScriptComponent::Teardown() {
    if (m_script && owner && m_scriptCreated) {
        m_script->OnDestroy(*owner);
        m_scriptCreated = false;
    }
}

void ScriptComponent::OnDetach() {
    Teardown();
}

void ScriptComponent::Update(double elapsed_time) {
    if (!m_script || !owner) return;

    // Call OnCreate lazily on the first Update() while the simulation is playing.
    // This prevents scripts from running game logic (e.g. Enemy_Spawn) in editor mode.
    if (!m_scriptCreated) {
        if (!EngineGUI_IsPlaying()) return;
        m_script->OnCreate(*owner);
        m_scriptCreated = true;
    }

    m_script->OnUpdate(*owner, static_cast<float>(elapsed_time));
}

void ScriptComponent::OnReset() {
    Teardown();
    // Re-apply original prefab properties so scripts start fresh
    if (m_script && m_scriptProperties.is_object() && !m_scriptProperties.empty())
        m_script->Deserialize(m_scriptProperties);
}

// „Ÿ„Ÿ Serialization „Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ

std::string ScriptComponent::Serialize() const {
    nlohmann::json j;
    j["scriptClass"] = m_scriptClass;
    if (m_script) {
        nlohmann::json props =
            ScriptFactory::Instance().SerializeScriptProperties(m_scriptClass, m_script.get());
        nlohmann::json defaults = nlohmann::json::object();
        if (auto defaultScript = ScriptFactory::Instance().Create(m_scriptClass)) {
            defaults = ScriptFactory::Instance().SerializeScriptProperties(
                m_scriptClass, defaultScript.get());
        }
        nlohmann::json overrides = nlohmann::json::object();
        for (auto& [key, val] : props.items()) {
            if (!defaults.contains(key) || defaults[key] != val)
                overrides[key] = val;
        }
        if (!overrides.empty()) j["properties"] = overrides;
    } else if (!m_scriptProperties.is_null() && !m_scriptProperties.empty()) {
        j["properties"] = m_scriptProperties;
    }
    return j.dump();
}

nlohmann::json ScriptComponent::GetEditableProperties() const {
    if (m_script)
        return ScriptFactory::Instance().SerializeScriptProperties(m_scriptClass, m_script.get());
    return m_scriptProperties.is_object() ? m_scriptProperties : nlohmann::json::object();
}

void ScriptComponent::ApplyProperties(const nlohmann::json& properties) {
    m_scriptProperties = properties.is_object() ? properties : nlohmann::json::object();
    if (m_script)
        ScriptFactory::Instance().DeserializeScriptProperties(
            m_scriptClass, m_script.get(), m_scriptProperties);
}

void ScriptComponent::Deserialize(const std::string& jsonStr) {
    try {
        auto j = nlohmann::json::parse(jsonStr);
        m_scriptClass      = j.value("scriptClass", "");
        m_scriptProperties = j.contains("properties") ? j["properties"] : nlohmann::json{};
        CreateScript();
    } catch (...) {
        OutputDebugStringA("[ScriptComponent] Deserialize: JSON parse error.\n");
    }
}

// „Ÿ„Ÿ Internal helper „Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ

void ScriptComponent::CreateScript() {
    m_script.reset();
    m_scriptCreated = false;
    if (m_scriptClass.empty()) return;

    m_script = ScriptFactory::Instance().Create(m_scriptClass);
    if (!m_script) {
        std::string msg = "[ScriptComponent] No script registered for scriptClass: "
                          + m_scriptClass + "\n";
        OutputDebugStringA(msg.c_str());
        return;
    }

    if (m_scriptProperties.is_object() && !m_scriptProperties.empty()) {
        ScriptFactory::Instance().DeserializeScriptProperties(
            m_scriptClass, m_script.get(), m_scriptProperties);
    } else {
        m_scriptProperties =
            ScriptFactory::Instance().SerializeScriptProperties(m_scriptClass, m_script.get());
    }
}

void ScriptComponent::HotReload() {
    // Tear down the old script instance safely
    if (m_script && owner && m_scriptCreated) {
        m_script->OnDestroy(*owner);
        m_scriptCreated = false;
    }
    m_script.reset();   // destructor runs, old DLL memory freed

    // CreateScript() calls ScriptFactory::Create() which now returns
    // an instance from the newly loaded DLL ? everything else is the same
    CreateScript();

    // OnCreate will fire naturally on next Update() via the existing lazy init
}


