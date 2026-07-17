#pragma once

#include "libraries/nlohmann/json.hpp"
#include "../scripting/ui_canvas_script_factory.h"
#include <memory>
#include <string>

class UIElement;
class UICanvasScript;

class UIElementScriptHost {
public:
    void SetScriptClass(const std::string& scriptClass);
    void SetProperties(const nlohmann::json& properties);
    void LoadFromJson(const nlohmann::json& scriptJson);
    nlohmann::json SaveToJson() const;

    const std::string& GetScriptClass() const { return m_scriptClass; }
    UICanvasScript* GetScript() const { return m_script.get(); }

    void Update(UIElement& owner, double elapsed_time);
    void Teardown(UIElement& owner);

private:
    void CreateScript();

    std::string m_scriptClass;
    nlohmann::json m_scriptProperties = nlohmann::json::object();
    UICanvasScriptFactory::ScriptPtr m_script;
    bool m_scriptCreated = false;
};
