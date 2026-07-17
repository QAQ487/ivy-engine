#include "script_watcher.h"
#include <filesystem>

void ScriptWatcher::Start(const std::string& watchDir, Callback onChange) {
    m_running = true;
    m_stopEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);

    m_thread = std::thread([this, watchDir, onChange]() {
        HANDLE hDir = CreateFileA(
            watchDir.c_str(), FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);

        if (hDir == INVALID_HANDLE_VALUE) return;

        char buf[4096];
        OVERLAPPED ov = {};
        ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        HANDLE handles[2] = { ov.hEvent, m_stopEvent };

        while (m_running) {
            ReadDirectoryChangesW(hDir, buf, sizeof(buf), FALSE,
                FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
                nullptr, &ov, nullptr);

            DWORD wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
            if (wait != WAIT_OBJECT_0) break;  // stop event or error

            DWORD bytes = 0;
            GetOverlappedResult(hDir, &ov, &bytes, FALSE);
            ResetEvent(ov.hEvent);

            auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buf);
            do {
                std::wstring wname(info->FileName, info->FileNameLength / 2);
                std::filesystem::path p = std::filesystem::path(watchDir) / wname;

                if (p.extension() == L".dll" &&
                    p.filename().wstring().find(L".hot_") == std::wstring::npos) {
                    std::string name = p.stem().string();   // "EnemyScript"
                    std::string path = p.string();
                    // Small delay — compiler may still be writing the file
                    Sleep(100);
                    onChange(name, path);
                }

                if (!info->NextEntryOffset) break;
                info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                    reinterpret_cast<char*>(info) + info->NextEntryOffset);
            } while (true);
        }

        CloseHandle(ov.hEvent);
        CloseHandle(hDir);
        });
}

void ScriptWatcher::Stop() {
    m_running = false;
    if (m_stopEvent) {
        SetEvent(m_stopEvent);
        if (m_thread.joinable()) m_thread.join();
        CloseHandle(m_stopEvent);
        m_stopEvent = nullptr;
    }
}