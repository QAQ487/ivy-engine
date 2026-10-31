// ScriptDllExports.cpp
#include "Script.h"
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>

// ── DLL-local registry ────────────────────────────────────────────────────────
// This is separate from the engine's ScriptFactory. It lives entirely inside
// the DLL and is only accessed through the CreateScript export below.

using DllCreator = std::function<Script* ()>;
static std::unordered_map<std::string, DllCreator>& GetDllRegistry() {
    static std::unordered_map<std::string, DllCreator> s_registry;
    return s_registry;
}

// Called by REGISTER_SCRIPT at static-init time inside the DLL.
void DllScript_Register(const std::string& name, DllCreator creator) {
    GetDllRegistry()[name] = std::move(creator);
}

// ── Single export ─────────────────────────────────────────────────────────────
// ScriptFactory calls CreateScript with the class name it wants to instantiate
// and uses EnumerateScripts to discover the names exported by this DLL.

extern "C" __declspec(dllexport)
Script* CreateScript(const char* className) {
    if (!className) return nullptr;
    auto& reg = GetDllRegistry();
    auto it = reg.find(className);
    return (it != reg.end()) ? it->second() : nullptr;
}

extern "C" __declspec(dllexport)
void DestroyScript(Script* script) {
    delete script;
}

extern "C" __declspec(dllexport)
bool SerializeScript(const Script* script, char* outJson, int outJsonSize) {
    if (!script || !outJson || outJsonSize <= 0)
        return false;

    const std::string jsonText = script->Serialize().dump();
    if (jsonText.size() + 1 > static_cast<size_t>(outJsonSize)) {
        outJson[0] = '\0';
        return false;
    }

    strcpy_s(outJson, static_cast<size_t>(outJsonSize), jsonText.c_str());
    return true;
}

extern "C" __declspec(dllexport)
void DeserializeScript(Script* script, const char* jsonText) {
    if (!script || !jsonText)
        return;

    try {
        script->Deserialize(nlohmann::json::parse(jsonText));
    } catch (...) {
        script->Deserialize(nlohmann::json::object());
    }
}

extern "C" __declspec(dllexport)
void EnumerateScripts(void(*callback)(const char*)) {
    for (const auto& kv : GetDllRegistry())
        callback(kv.first.c_str());
}