#include "ui_element_script_host.h"
#include "ui_element.h"
#include "../core/engine_gui.h"
#include <Windows.h>

void UIElementScriptHost::SetScriptClass(const std::string& scriptClass) {
    if (m_scriptClass == scriptClass)
        return;
    m_scriptClass = scriptClass;
    CreateScript();
}

void UIElementScriptHost::SetProperties(const nlohmann::json& properties) {
    m_scriptProperties = properties.is_object() ? properties : nlohmann::json::object();
    if (m_script && m_scriptProperties.is_object() && !m_scriptProperties.empty())
        m_script->Deserialize(m_scriptProperties);
}

void UIElementScriptHost::LoadFromJson(const nlohmann::json& scriptJson) {
    if (!scriptJson.is_object()) {
        m_scriptClass.clear();
        m_scriptProperties = nlohmann::json::object();
        m_script.reset();
        m_scriptCreated = false;
        return;
    }

    m_scriptClass = scriptJson.value("scriptClass", "");
    m_scriptProperties = scriptJson.contains("properties") ? scriptJson["properties"] : nlohmann::json::object();
    CreateScript();
}

nlohmann::json UIElementScriptHost::SaveToJson() const {
    if (m_scriptClass.empty())
        return nlohmann::json();

    nlohmann::json scriptJson;
    scriptJson["scriptClass"] = m_scriptClass;
    if (m_script) {
        nlohmann::json props = m_script->Serialize();
        if (props.is_object() && !props.empty())
            scriptJson["properties"] = props;
    } else if (m_scriptProperties.is_object() && !m_scriptProperties.empty()) {
        scriptJson["properties"] = m_scriptProperties;
    }
    return scriptJson;
}

void UIElementScriptHost::CreateScript() {
    m_script.reset();
    m_scriptCreated = false;
    if (m_scriptClass.empty())
        return;

    m_script = UICanvasScriptFactory::Instance().Create(m_scriptClass);
    if (!m_script) {
        std::string msg = "[UIElementScriptHost] No UI script registered for scriptClass: "
            + m_scriptClass + "\n";
        OutputDebugStringA(msg.c_str());
        return;
    }

    if (m_scriptProperties.is_object() && !m_scriptProperties.empty())
        m_script->Deserialize(m_scriptProperties);
}

void UIElementScriptHost::Update(UIElement& owner, double elapsed_time) {
    if (!m_script)
        return;

    if (!m_scriptCreated) {
        if (!EngineGUI_IsPlaying())
            return;
        m_script->OnCreate(owner);
        m_scriptCreated = true;
    }

    m_script->OnUpdate(owner, static_cast<float>(elapsed_time));
}

void UIElementScriptHost::Teardown(UIElement& owner) {
    if (m_script && m_scriptCreated) {
        m_script->OnDestroy(owner);
        m_scriptCreated = false;
    }
}
