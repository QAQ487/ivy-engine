/*****************************************************
 * File: engine/scripting/ScriptFactory.cpp
 * Project: d3d11_engine
 * Author: Terry Yeung
 * Created: 2026-03-13
 * Description: ScriptFactory implementation.
 *****************************************************/
#include "script_factory.h"
#include "ui_canvas_script_factory.h"
#include <algorithm>
#include <Windows.h>
#include "engine/debug_draw/debug_ofstream.h"

namespace {
    std::vector<std::string>* g_enumeratedScripts = nullptr;
    ScriptFactory::ReloadFeedback g_reloadFeedback;

    void CollectScriptName(const char* name) {
        if (g_enumeratedScripts && name && *name)
            g_enumeratedScripts->push_back(name);
    }

    std::string GetLastErrorText(const char* apiName) {
        return std::string(apiName) + " failed with error "
            + std::to_string(GetLastError());
    }
}

ScriptFactory& ScriptFactory::Instance() {
    static ScriptFactory inst;
    return inst;
}

ScriptFactory::DllEntry::~DllEntry() {
    if (handle) {
        FreeLibrary(handle);
        handle = nullptr;
    }
    if (!tempPath.empty())
        DeleteFileA(tempPath.c_str());
}

void ScriptFactory::Register(const std::string& name, Creator creator) {
    m_creators[name] = std::move(creator);
}

ScriptFactory::ScriptPtr ScriptFactory::Create(const std::string& name) const {
    auto it = m_creators.find(name);
    if (it != m_creators.end()) return it->second();
    std::string msg = "[ScriptFactory] No script registered for class name: "
                      + name + "\n";
    hal::DebugWriteA(msg.c_str());
    return ScriptPtr(nullptr, [](Script*) {});
}

bool ScriptFactory::IsRegistered(const std::string& name) const {
    return m_creators.count(name) > 0;
}

bool ScriptFactory::IsDllScript(const std::string& name) const {
    return m_dlls.count(name) > 0;
}

nlohmann::json ScriptFactory::SerializeScriptProperties(
    const std::string& name,
    const Script* script) const
{
    if (!script) return nlohmann::json::object();

    auto dllIt = m_dllApis.find(name);
    if (dllIt == m_dllApis.end() || !dllIt->second.serialize)
        return script->Serialize();

    // SerializeScript reports failure (without indicating the required size)
    // when the buffer is too small, so retry with a growing buffer instead of
    // silently truncating/dropping properties that exceed a fixed cap.
    constexpr size_t kMaxBufferSize = 16 * 1024 * 1024; // 16MB hard ceiling
    for (size_t bufferSize = 16 * 1024; bufferSize <= kMaxBufferSize; bufferSize *= 4) {
        std::vector<char> buffer(bufferSize);
        if (!dllIt->second.serialize(script, buffer.data(), static_cast<int>(buffer.size())))
            continue;

        try {
            return nlohmann::json::parse(buffer.data());
        } catch (...) {
            hal::DebugWriteA(("[ScriptFactory] Script Serialize returned invalid JSON: "
                + name + "\n").c_str());
            return nlohmann::json::object();
        }
    }

    hal::DebugWriteA(("[ScriptFactory] Script properties exceed "
        + std::to_string(kMaxBufferSize) + " bytes, giving up: " + name + "\n").c_str());
    return nlohmann::json::object();
}

void ScriptFactory::DeserializeScriptProperties(
    const std::string& name,
    Script* script,
    const nlohmann::json& properties) const
{
    if (!script) return;

    auto dllIt = m_dllApis.find(name);
    if (dllIt == m_dllApis.end() || !dllIt->second.deserialize) {
        script->Deserialize(properties);
        return;
    }

    const std::string jsonText = properties.is_object()
        ? properties.dump()
        : nlohmann::json::object().dump();
    dllIt->second.deserialize(script, jsonText.c_str());
}

bool ScriptFactory::LoadScriptDll(const std::string& name,
    const std::string& dllPath,
    std::vector<std::string>* loadedScripts) {
    static int s_gen = 0;
    std::string tempPath = dllPath + ".hot_" + std::to_string(s_gen++);

    bool copied = false;
    for (int attempt = 0; attempt < 10; ++attempt) {
        if (CopyFileA(dllPath.c_str(), tempPath.c_str(), FALSE)) {
            copied = true;
            break;
        }
        Sleep(50);
    }
    if (!copied) {
        hal::DebugWriteA(("[ScriptFactory] " + GetLastErrorText("CopyFileA")
            + ": " + dllPath + "\n").c_str());
        return false;
    }

    HMODULE h = LoadLibraryA(tempPath.c_str());
    if (!h) {
        hal::DebugWriteA(("[ScriptFactory] " + GetLastErrorText("LoadLibraryA")
            + ": " + tempPath + "\n").c_str());
        DeleteFileA(tempPath.c_str());
        return false;
    }

    using CreateFn = Script * (*)(const char*);
    auto fn = reinterpret_cast<CreateFn>(GetProcAddress(h, "CreateScript"));
    if (!fn) {
        FreeLibrary(h);
        DeleteFileA(tempPath.c_str());
        hal::DebugWriteA(("[ScriptFactory] No CreateScript export: " + dllPath + "\n").c_str());
        return false;
    }

    using DestroyFn = void (*)(Script*);
    auto destroyFn = reinterpret_cast<DestroyFn>(GetProcAddress(h, "DestroyScript"));
    if (!destroyFn) {
        FreeLibrary(h);
        DeleteFileA(tempPath.c_str());
        hal::DebugWriteA(("[ScriptFactory] No DestroyScript export: " + dllPath + "\n").c_str());
        return false;
    }

    auto serializeFn = reinterpret_cast<SerializeScriptFn>(
        GetProcAddress(h, "SerializeScript"));
    auto deserializeFn = reinterpret_cast<DeserializeScriptFn>(
        GetProcAddress(h, "DeserializeScript"));
    if (!serializeFn || !deserializeFn) {
        FreeLibrary(h);
        DeleteFileA(tempPath.c_str());
        hal::DebugWriteA(("[ScriptFactory] Missing script property exports: "
            + dllPath + "\n").c_str());
        return false;
    }

    std::vector<std::string> scriptNames;
    using EnumerateFn = void (*)(void (*)(const char*));
    auto enumerate = reinterpret_cast<EnumerateFn>(GetProcAddress(h, "EnumerateScripts"));
    if (enumerate) {
        g_enumeratedScripts = &scriptNames;
        enumerate(&CollectScriptName);
        g_enumeratedScripts = nullptr;
        std::sort(scriptNames.begin(), scriptNames.end());
        scriptNames.erase(std::unique(scriptNames.begin(), scriptNames.end()), scriptNames.end());
    }
    if (scriptNames.empty())
        scriptNames.push_back(name);

    auto module = std::make_shared<DllEntry>();
    module->handle = h;
    module->tempPath = tempPath;

    for (const std::string& scriptName : scriptNames) {
        if (scriptName.empty()) continue;

        auto old = m_dlls.find(scriptName);
        if (old != m_dlls.end() && old->second != module) {
            if (std::find(m_retiredDlls.begin(), m_retiredDlls.end(), old->second)
                == m_retiredDlls.end()) {
                m_retiredDlls.push_back(old->second);
            }
        }

        m_dlls[scriptName] = module;
        m_dllApis[scriptName] = { serializeFn, deserializeFn };
        Register(scriptName, [fn, destroyFn, scriptName, module]() {
            Script* script = fn(scriptName.c_str());
            return ScriptPtr(script, [destroyFn, module](Script* s) {
                if (s)
                    destroyFn(s);
            });
        });
        if (loadedScripts)
            loadedScripts->push_back(scriptName);
        hal::DebugWriteA(("[ScriptFactory] Loaded: " + scriptName + "\n").c_str());
    }

    // Pass the same shared_ptr<DllEntry> that pins this module alive for
    // regular scripts, so UI canvas scripts loaded from this DLL keep it
    // mapped for exactly as long as needed too (see LoadFromModule comment).
    UICanvasScriptFactory::Instance().LoadFromModule(h, name, module);
    return true;
}

void ScriptFactory::UnloadScriptDll(const std::string& name) {
    auto it = m_dlls.find(name);
    if (it == m_dlls.end()) return;
    if (std::find(m_retiredDlls.begin(), m_retiredDlls.end(), it->second)
        == m_retiredDlls.end()) {
        m_retiredDlls.push_back(it->second);
    }
    m_dlls.erase(it);
    m_dllApis.erase(name);
    m_creators.erase(name);
}

void ScriptFactory::ReleaseRetiredDlls() {
    m_retiredDlls.clear();
}

bool ScriptFactory::InvokeExportOnLoadedDlls(const char* exportName) const {
    if (!exportName || !exportName[0]) return false;

    std::vector<HMODULE> handles;
    handles.reserve(m_dlls.size());
    for (const auto& kv : m_dlls) {
        if (!kv.second || !kv.second->handle) continue;
        if (std::find(handles.begin(), handles.end(), kv.second->handle) != handles.end())
            continue;
        handles.push_back(kv.second->handle);
    }

    bool invoked = false;
    for (HMODULE handle : handles) {
        auto fn = reinterpret_cast<void(*)()>(GetProcAddress(handle, exportName));
        if (fn) {
            fn();
            invoked = true;
        }
    }
    return invoked;
}

void* ScriptFactory::ResolveDllExport(const char* exportName) const {
    if (!exportName || !exportName[0])
        return nullptr;

    std::vector<HMODULE> handles;
    handles.reserve(m_dlls.size());
    for (const auto& kv : m_dlls) {
        if (!kv.second || !kv.second->handle)
            continue;
        if (std::find(handles.begin(), handles.end(), kv.second->handle) != handles.end())
            continue;
        handles.push_back(kv.second->handle);
    }

    for (HMODULE handle : handles) {
        if (void* fn = reinterpret_cast<void*>(GetProcAddress(handle, exportName)))
            return fn;
    }
    return nullptr;
}

void ScriptFactory::UnloadAllDlls() {
    std::vector<std::string> scriptNames;
    scriptNames.reserve(m_dlls.size());
    for (const auto& kv : m_dlls)
        scriptNames.push_back(kv.first);

    std::vector<std::shared_ptr<DllEntry>> modules;
    modules.reserve(m_dlls.size());
    for (const auto& kv : m_dlls) {
        if (kv.second && std::find(modules.begin(), modules.end(), kv.second) == modules.end())
            modules.push_back(kv.second);
    }

    m_dlls.clear();
    m_dllApis.clear();
    for (const std::string& name : scriptNames)
        m_creators.erase(name);

    for (const auto& module : modules) {
        if (std::find(m_retiredDlls.begin(), m_retiredDlls.end(), module) == m_retiredDlls.end())
            m_retiredDlls.push_back(module);
    }
    ReleaseRetiredDlls();
}

void ScriptFactory::RequestReload(const std::string& name,
    const std::string& path) {
    std::lock_guard<std::mutex> lk(m_reloadMutex);
    // Overwrite any existing pending reload for this name
    for (auto& p : m_pendingReloads) {
        if (p.name == name) { p.path = path; p.observed = std::chrono::steady_clock::now(); return; }
    }
    m_pendingReloads.push_back({ name, path });
    hal::DebugWriteA(("[ScriptFactory] Queued reload: " + name
        + " -> " + path + "\n").c_str());
}

size_t ScriptFactory::PendingReloadCount() const {
    std::lock_guard<std::mutex> lk(m_reloadMutex);
    return m_pendingReloads.size();
}

const ScriptFactory::ReloadFeedback& ScriptFactory::GetLastReloadFeedback() const {
    return g_reloadFeedback;
}

void ScriptFactory::DrainReloadQueue(
    const std::function<void(const std::string&)>& onReloaded)
{
    std::vector<PendingReload> queue;
    {
        std::lock_guard<std::mutex> lk(m_reloadMutex);
        const auto now = std::chrono::steady_clock::now();
        auto it = m_pendingReloads.begin();
        while (it != m_pendingReloads.end()) {
            if (now - it->observed >= std::chrono::milliseconds(500)) { queue.push_back(*it); it = m_pendingReloads.erase(it); }
            else ++it;
        }
    }
    if (queue.empty()) return;
    ReloadFeedback feedback;
    feedback.revision = g_reloadFeedback.revision + 1;
    for (const auto& r : queue) {
        std::vector<std::string> loadedScripts;
        if (LoadScriptDll(r.name, r.path, &loadedScripts)) {
            for (const std::string& scriptName : loadedScripts)
                onReloaded(scriptName);
            ReleaseRetiredDlls();
        } else {
            feedback.failed = true;
            if (!feedback.error.empty()) feedback.error += "\n";
            feedback.error += "Cannot reload " + r.name + " from " + r.path + ". See debug output for details.";
        }
    }
    g_reloadFeedback = std::move(feedback);
}
