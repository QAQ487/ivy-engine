// ui_canvas_script_factory.cpp
// Factory for UI-canvas-bound scripts.

#include "ui_canvas_script_factory.h"

#include <algorithm>
#include "engine/debug_draw/debug_ofstream.h"

namespace {
    std::vector<std::string>* g_enumeratedUiScripts = nullptr;

    void CollectUiScriptName(const char* name) {
        if (g_enumeratedUiScripts && name && *name)
            g_enumeratedUiScripts->push_back(name);
    }
}

UICanvasScriptFactory& UICanvasScriptFactory::Instance() {
    static UICanvasScriptFactory inst;
    return inst;
}

void UICanvasScriptFactory::Register(const std::string& name, Creator creator) {
    m_creators[name] = std::move(creator);
}

UICanvasScriptFactory::ScriptPtr UICanvasScriptFactory::Create(const std::string& name) const {
    auto it = m_creators.find(name);
    if (it != m_creators.end())
        return it->second();

    std::string msg = "[UICanvasScriptFactory] No script registered for class name: "
        + name + "\n";
    hal::DebugWriteA(msg.c_str());
    return ScriptPtr(nullptr, [](UICanvasScript*) {});
}

bool UICanvasScriptFactory::IsRegistered(const std::string& name) const {
    return m_creators.count(name) > 0;
}

std::vector<std::string> UICanvasScriptFactory::GetRegisteredNames() const {
    std::vector<std::string> names;
    names.reserve(m_creators.size());
    for (const auto& entry : m_creators)
        names.push_back(entry.first);
    std::sort(names.begin(), names.end());
    return names;
}

void UICanvasScriptFactory::LoadFromModule(HMODULE module, const std::string& fallbackName,
                                            std::shared_ptr<void> keepAlive) {
    if (!module)
        return;

    using CreateFn = UICanvasScript * (*)(const char*);
    auto createFn = reinterpret_cast<CreateFn>(GetProcAddress(module, "CreateUICanvasScript"));
    if (!createFn)
        return;

    using DestroyFn = void (*)(UICanvasScript*);
    auto destroyFn = reinterpret_cast<DestroyFn>(GetProcAddress(module, "DestroyUICanvasScript"));
    if (!destroyFn)
        return;

    std::vector<std::string> scriptNames;
    using EnumerateFn = void (*)(void (*)(const char*));
    auto enumerateFn = reinterpret_cast<EnumerateFn>(GetProcAddress(module, "EnumerateUICanvasScripts"));
    if (enumerateFn) {
        g_enumeratedUiScripts = &scriptNames;
        enumerateFn(&CollectUiScriptName);
        g_enumeratedUiScripts = nullptr;
        std::sort(scriptNames.begin(), scriptNames.end());
        scriptNames.erase(std::unique(scriptNames.begin(), scriptNames.end()), scriptNames.end());
    }
    if (scriptNames.empty() && !fallbackName.empty())
        scriptNames.push_back(fallbackName);

    for (const std::string& scriptName : scriptNames) {
        if (scriptName.empty())
            continue;

        Register(scriptName, [createFn, destroyFn, scriptName, keepAlive]() {
            UICanvasScript* script = createFn(scriptName.c_str());
            return ScriptPtr(script, [destroyFn, keepAlive](UICanvasScript* s) {
                if (s)
                    destroyFn(s);
            });
        });
    }
}
