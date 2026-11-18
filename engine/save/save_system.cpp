// save_system.cpp
// Windows layer for the save store: picks the folder and replaces files with
// MoveFileExW. See save_system.h.

#include "engine/save/save_system.h"
#include "engine/save/save_store.h"
#include "engine/core/utf8_path.h"
#include "engine/debug_draw/debug_ofstream.h"
#include "libraries/nlohmann/json.hpp"

#include <Windows.h>
#include <KnownFolders.h>
#include <ShlObj.h>

#include <fstream>
#include <memory>
#include <string>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace fs = std::filesystem;

namespace {

std::unique_ptr<save_store::Store> g_store;

void LogLine(const std::string& line) {
    hal::DebugWriteA((line + "\n").c_str());
}

std::string LastErrorText() {
    return "Windows error " + std::to_string(GetLastError());
}

bool WinReplaceFile(const fs::path& tmp, const fs::path& target, const fs::path& backup,
                    std::string* error) {
    std::error_code ec;
    if (!backup.empty() && fs::exists(target, ec)) {
        if (!CopyFileW(target.c_str(), backup.c_str(), FALSE)) {
            if (error) *error = "backup copy failed (" + LastErrorText() + ")";
            return false;
        }
    }
    if (!MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        if (error) *error = "replace failed (" + LastErrorText() + ")";
        return false;
    }
    return true;
}

#if defined(GAME_RUNTIME)
fs::path KnownFolder(REFKNOWNFOLDERID id) {
    PWSTR raw = nullptr;
    fs::path out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &raw)) && raw)
        out = fs::path(raw);
    if (raw)
        CoTaskMemFree(raw);
    return out;
}

fs::path ExeDirectory() {
    wchar_t buf[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return (len > 0 && len < MAX_PATH) ? fs::path(buf).parent_path() : fs::path();
}

// save.folderName, else name, from project.json in the working directory or
// beside the exe (same lookup as the window title).
std::string ProjectSaveFolderName() {
    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    for (const fs::path& candidate : {cwd / "project.json", ExeDirectory() / "project.json"}) {
        try {
            std::ifstream in(candidate);
            if (!in)
                continue;
            const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
            if (!j.is_object())
                continue;
            if (const auto save = j.find("save"); save != j.end() && save->is_object()) {
                if (const auto f = save->find("folderName"); f != save->end() && f->is_string() &&
                    !f->get<std::string>().empty())
                    return f->get<std::string>();
            }
            if (const auto name = j.find("name"); name != j.end() && name->is_string())
                return name->get<std::string>();
        } catch (...) {
        }
    }
    return {};
}
#endif

fs::path ResolveSaveDirectory() {
#if defined(GAME_RUNTIME)
    const std::string folder = save_store::SanitizeFolderName(ProjectSaveFolderName(), "IvyGame");
    const fs::path name = PathFromUtf8(folder);
    if (fs::path base = KnownFolder(FOLDERID_SavedGames); !base.empty())
        return base / name;
    if (fs::path base = KnownFolder(FOLDERID_LocalAppData); !base.empty())
        return base / name / "saves";
    return ExeDirectory() / "saves";
#else
    // The editor runs with the content project as the working directory.
    std::error_code ec;
    return fs::current_path(ec) / ".ivy" / "saves";
#endif
}

}  // namespace

std::mutex& SaveSystem_Mutex() {
    static std::mutex m;
    return m;
}

save_store::Store& SaveSystem_Store() {
    if (!g_store) {
        const fs::path dir = ResolveSaveDirectory();
        g_store = std::make_unique<save_store::Store>(dir, &WinReplaceFile, &LogLine);
        LogLine("[Save] save folder: " + PathToUtf8(dir));
    }
    return *g_store;
}

void SaveSystem_Reset() {
    std::lock_guard<std::mutex> lock(SaveSystem_Mutex());
    g_store.reset();
}
