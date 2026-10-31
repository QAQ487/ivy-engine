// ui_canvas_script_factory.h
// Factory for UI-canvas-bound scripts.

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

    // keepAlive should be the same shared_ptr the owning ScriptFactory uses to
    // pin the DLL module (type-erased so this factory need not know about
    // ScriptFactory's private DllEntry). Every UICanvasScript instance created
    // from this module captures keepAlive, so the module stays mapped for as
    // long as any instance (or its deleter) might still touch its code,
    // preventing the hot-reload use-after-free/crash that FreeLibrary-ing the
    // module out from under a live instance would otherwise cause.
    void LoadFromModule(HMODULE module, const std::string& fallbackName,
                         std::shared_ptr<void> keepAlive);

private:
    UICanvasScriptFactory() = default;

    std::unordered_map<std::string, Creator> m_creators;
};
