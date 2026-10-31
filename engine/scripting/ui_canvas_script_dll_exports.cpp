// ui_canvas_script_dll_exports.cpp
// Scripting: ui canvas script dll exports implementation.

#include "ui_canvas_script.h"
#include <functional>
#include <string>
#include <unordered_map>

using DllUiCreator = std::function<UICanvasScript* ()>;

static std::unordered_map<std::string, DllUiCreator>& GetDllUiRegistry() {
    static std::unordered_map<std::string, DllUiCreator> s_registry;
    return s_registry;
}

void DllUICanvasScript_Register(const std::string& name, DllUiCreator creator) {
    GetDllUiRegistry()[name] = std::move(creator);
}

extern "C" __declspec(dllexport)
UICanvasScript* CreateUICanvasScript(const char* className) {
    if (!className)
        return nullptr;
    auto& reg = GetDllUiRegistry();
    auto it = reg.find(className);
    return (it != reg.end()) ? it->second() : nullptr;
}

extern "C" __declspec(dllexport)
void DestroyUICanvasScript(UICanvasScript* script) {
    delete script;
}

extern "C" __declspec(dllexport)
void EnumerateUICanvasScripts(void(*callback)(const char*)) {
    for (const auto& kv : GetDllUiRegistry())
        callback(kv.first.c_str());
}
