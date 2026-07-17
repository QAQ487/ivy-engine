#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <Windows.h>

// Watches a directory for .dll changes and fires a callback.
class ScriptWatcher {
public:
    using Callback = std::function<void(const std::string& scriptName,
        const std::string& dllPath)>;

    void Start(const std::string& watchDir, Callback onChange);
    void Stop();

    ~ScriptWatcher() { Stop(); }

private:
    std::thread     m_thread;
    std::atomic<bool> m_running{ false };
    HANDLE          m_stopEvent = nullptr;
};
