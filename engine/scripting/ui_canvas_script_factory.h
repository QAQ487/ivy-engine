#pragma once

#include "ui_canvas_script.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <Windows.h>

class UICanvasScriptFactory {
public:
    using ScriptPtr = std::unique_ptr<UICanvasScript, std::function<void(UICanvasScript*)>>;
    using Creator = std::function<ScriptPtr()>;

    static UICanvasScriptFactory& Instance();

    void Register(const std::string& name, Creator creator);
    ScriptPtr Create(const std::string& name) const;
    bool IsRegistered(const std::string& name) const;
    std::vector<std::string> GetRegisteredNames() const;
    void LoadFromModule(HMODULE module, const std::string& fallbackName);

private:
    UICanvasScriptFactory() = default;

    std::unordered_map<std::string, Creator> m_creators;
};
