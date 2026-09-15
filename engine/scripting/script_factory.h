/*****************************************************
 * File: engine/scripting/ScriptFactory.h
 * Project: d3d11_engine
 * Author: Terry Yeung
 * Created: 2026-03-13
 * Description: ScriptFactory.h ? Singleton registry that maps Script class
 *              name strings to creator functions, enabling JSON-driven script
 *              instantiation by class name without hard-coded switch/case logic.
 *
 * Usage:
 *   // Register (done automatically via REGISTER_SCRIPT macro):
 *   ScriptFactory::Instance().Register("EnemyScript",
 *       []{ return std::make_unique<EnemyScript>(); });
 *
 *   // Create by class name (used by ScriptComponent during prefab load):
 *   auto script = ScriptFactory::Instance().Create("EnemyScript");
 *****************************************************/
#pragma once

#include "Script.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <Windows.h>

// Singleton factory: maps Script class-name strings to creator lambdas.
class ScriptFactory {
public:
    using ScriptPtr = std::unique_ptr<Script, std::function<void(Script*)>>;
    using Creator = std::function<ScriptPtr()>;

    static ScriptFactory& Instance();

    // Register a creator for the given class name.
    // Re-registering the same name overwrites the previous creator.
    void Register(const std::string& name, Creator creator);

    // Create a Script instance by its registered class name.
    // Returns nullptr if the name has not been registered.
    ScriptPtr Create(const std::string& name) const;

    // Returns true if a creator is registered for the given class name.
    bool IsRegistered(const std::string& name) const;

    // Returns true when the script creator comes from a hot-loaded DLL.
    bool IsDllScript(const std::string& name) const;
    nlohmann::json SerializeScriptProperties(const std::string& name,
                                             const Script* script) const;
    void DeserializeScriptProperties(const std::string& name,
                                     Script* script,
                                     const nlohmann::json& properties) const;

    // DLL hot-reload API
    bool   LoadScriptDll(const std::string& scriptName, const std::string& dllPath,
                         std::vector<std::string>* loadedScripts = nullptr);
    void   UnloadScriptDll(const std::string& scriptName);
    void   UnloadAllDlls();
    // Called from watcher thread ? thread-safe, just queues the request.
    void RequestReload(const std::string& name, const std::string& dllPath);
    size_t PendingReloadCount() const;

    // Called from main thread each frame ? executes queued reloads.
    // Calls onReloaded(name) for each script that was successfully reloaded.
    void DrainReloadQueue(const std::function<void(const std::string&)>& onReloaded);

    // Releases old DLL copies after all script instances have been recreated.
    void ReleaseRetiredDlls();

    // Invokes an exported no-arg function on each distinct loaded script DLL.
    // Returns true if at least one module provided the export.
    bool InvokeExportOnLoadedDlls(const char* exportName) const;

    // Returns the first matching export from any loaded script DLL module.
    void* ResolveDllExport(const char* exportName) const;

private:
    ScriptFactory() = default;
    std::unordered_map<std::string, Creator> m_creators;

    struct DllEntry {
        HMODULE     handle = nullptr;
        std::string tempPath;

        ~DllEntry();
    };
    using SerializeScriptFn = bool (*)(const Script*, char*, int);
    using DeserializeScriptFn = void (*)(Script*, const char*);

    struct DllApi {
        SerializeScriptFn serialize = nullptr;
        DeserializeScriptFn deserialize = nullptr;
    };

    std::unordered_map<std::string, std::shared_ptr<DllEntry>> m_dlls;
    std::unordered_map<std::string, DllApi> m_dllApis;
    std::vector<std::shared_ptr<DllEntry>> m_retiredDlls;

    struct PendingReload {
        std::string name;
        std::string path;
    };
    std::vector<PendingReload> m_pendingReloads;
    mutable std::mutex         m_reloadMutex;
};
