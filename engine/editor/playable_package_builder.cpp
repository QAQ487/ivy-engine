#include "playable_package_builder.h"

#include "project_manager.h"

#include "libraries/nlohmann/json.hpp"

#include <Windows.h>
#include <shlobj.h>

#include <atomic>
#include <cctype>
#include <cwctype>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace PlayablePackage {
namespace {

struct Job {
    std::mutex mutex;
    std::string log;
    std::string resultMessage;
    std::atomic<bool> running{false};
    std::atomic<bool> finished{false};
    std::atomic<bool> succeeded{false};
};

Job g_job;

std::wstring WideFromUtf8(const std::string& utf8) {
    if (utf8.empty())
        return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    if (size <= 0)
        return {};
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), out.data(), size);
    return out;
}

std::string Utf8FromWide(const std::wstring& wide) {
    if (wide.empty())
        return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return {};
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), out.data(), size,
                        nullptr, nullptr);
    return out;
}

std::string TrimCopy(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.pop_back();
    return value;
}

// Filesystem-safe exe stem from UI-supplied game name (worker must not touch ProjectManager).
std::string SanitizeGameExeStem(std::string name) {
    name = TrimCopy(std::move(name));
    std::string out;
    out.reserve(name.size());
    for (unsigned char c : name) {
        if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' ||
            c == '|' || c == '?' || c == '*')
            continue;
        out.push_back(static_cast<char>(c));
    }
    out = TrimCopy(std::move(out));
    if (out.empty())
        return "IvyEngine";
    return out;
}

fs::path NormalizeForCompare(const fs::path& path) {
    std::error_code ec;
    fs::path abs = fs::absolute(path, ec);
    if (ec)
        abs = path;
    abs = abs.lexically_normal();
    std::wstring s = abs.wstring();
    while (s.size() > 1 && (s.back() == L'\\' || s.back() == L'/'))
        s.pop_back();
    for (wchar_t& c : s) {
        if (c == L'/')
            c = L'\\';
        c = static_cast<wchar_t>(towlower(c));
    }
    return fs::path(s);
}

bool PathsEqual(const fs::path& a, const fs::path& b) {
    return NormalizeForCompare(a) == NormalizeForCompare(b);
}

bool IsUnderOrEqual(const fs::path& candidate, const fs::path& root) {
    const std::wstring c = NormalizeForCompare(candidate).wstring();
    const std::wstring r = NormalizeForCompare(root).wstring();
    if (c == r)
        return true;
    if (c.size() <= r.size())
        return false;
    if (c.compare(0, r.size(), r) != 0)
        return false;
    return c[r.size()] == L'\\';
}

std::string PrefsKey(const std::string& contentRoot) {
    return NormalizeForCompare(contentRoot).string();
}

std::string PrefsFilePath() {
    wchar_t* appData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
        std::string path = Utf8FromWide(appData);
        CoTaskMemFree(appData);
        path += "/GameEngine/build_prefs.json";
        return path;
    }
    return ProjectManager::GetExecutableDirectory() + "/build_prefs.json";
}

void AppendLog(const std::string& text) {
    std::lock_guard<std::mutex> lock(g_job.mutex);
    g_job.log += text;
}

void AppendLogLine(const std::string& text) {
    AppendLog(text);
    if (text.empty() || text.back() != '\n')
        AppendLog("\n");
}

std::string FindMsBuild() {
    wchar_t programFilesX86[MAX_PATH] = {};
    const DWORD n = GetEnvironmentVariableW(L"ProgramFiles(x86)", programFilesX86, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return {};

    const std::wstring vswhere = std::wstring(programFilesX86) +
        L"\\Microsoft Visual Studio\\Installer\\vswhere.exe";
    if (GetFileAttributesW(vswhere.c_str()) == INVALID_FILE_ATTRIBUTES)
        return {};

    const std::wstring cmd =
        L"\"" + vswhere +
        L"\" -latest -requires Microsoft.Component.MSBuild -find MSBuild\\**\\Bin\\MSBuild.exe";

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0))
        return {};
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::wstring mutableCmd = cmd;
    const BOOL ok = CreateProcessW(
        nullptr, mutableCmd.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(writePipe);
    if (!ok) {
        CloseHandle(readPipe);
        return {};
    }

    std::string output;
    char buffer[512];
    DWORD read = 0;
    while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0)
        output.append(buffer, buffer + read);
    CloseHandle(readPipe);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    const auto lineEnd = output.find_first_of("\r\n");
    if (lineEnd != std::string::npos)
        output.resize(lineEnd);
    output = TrimCopy(output);
    return output;
}

DWORD RunProcess(const std::wstring& commandLine, const std::wstring& workingDir) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
        AppendLogLine("Failed to create stdout pipe.");
        return static_cast<DWORD>(-1);
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::wstring mutableCmd = commandLine;
    const BOOL ok = CreateProcessW(
        nullptr, mutableCmd.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr,
        workingDir.empty() ? nullptr : workingDir.c_str(),
        &si, &pi);
    CloseHandle(writePipe);
    if (!ok) {
        CloseHandle(readPipe);
        AppendLogLine("CreateProcess failed (" + std::to_string(GetLastError()) + ").");
        return static_cast<DWORD>(-1);
    }

    char buffer[1024];
    DWORD read = 0;
    while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0)
        AppendLog(std::string(buffer, buffer + read));
    CloseHandle(readPipe);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = static_cast<DWORD>(-1);
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode;
}

bool CopyOneFile(const fs::path& from, const fs::path& to, std::string* errorOut) {
    std::error_code ec;
    if (!fs::exists(from, ec) || !fs::is_regular_file(from, ec)) {
        if (errorOut)
            *errorOut = "Missing file: " + from.string();
        return false;
    }
    fs::create_directories(to.parent_path(), ec);
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        if (errorOut)
            *errorOut = "Failed to copy " + from.string() + ": " + ec.message();
        return false;
    }
    return true;
}

bool CopyDirectoryRecursive(const fs::path& src, const fs::path& dst, std::string* errorOut) {
    std::error_code ec;
    if (!fs::exists(src, ec) || !fs::is_directory(src, ec)) {
        if (errorOut)
            *errorOut = "Source directory missing: " + src.string();
        return false;
    }

    fs::create_directories(dst, ec);
    for (fs::recursive_directory_iterator it(src, ec), end; it != end; it.increment(ec)) {
        if (ec) {
            if (errorOut)
                *errorOut = ec.message();
            return false;
        }
        const fs::path& from = it->path();
        const fs::path rel = fs::relative(from, src, ec);
        if (ec) {
            if (errorOut)
                *errorOut = ec.message();
            return false;
        }
        const fs::path to = dst / rel;
        if (it->is_directory(ec)) {
            fs::create_directories(to, ec);
        }
        else if (it->is_regular_file(ec)) {
            fs::create_directories(to.parent_path(), ec);
            fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                if (errorOut)
                    *errorOut = "Failed to copy " + from.string() + ": " + ec.message();
                return false;
            }
        }
        if (ec) {
            if (errorOut)
                *errorOut = ec.message();
            return false;
        }
    }
    return true;
}

bool AssemblePackage(const Request& request, const fs::path& engineRoot,
                     const std::string& configName, std::string* errorOut) {
    const fs::path content = request.contentRoot;
    const fs::path dest = request.destination;
    const fs::path engineOut = engineRoot / "x64" / configName;

    std::error_code ec;
    fs::create_directories(dest, ec);
    if (ec) {
        if (errorOut)
            *errorOut = "Failed to create destination: " + ec.message();
        return false;
    }

    const std::string exeStem = SanitizeGameExeStem(request.gameName);
    const std::string exeFile = exeStem + ".exe";
    if (!CopyOneFile(engineOut / "IvyEngineRuntime.exe", dest / exeFile, errorOut))
        return false;
    if (!CopyOneFile(engineOut / "libfbxsdk.dll", dest / "libfbxsdk.dll", errorOut))
        return false;
    if (!CopyOneFile(engineRoot / "libraries" / "box2d" / "LICENSE",
                     dest / "licenses" / "Box2D.txt", errorOut))
        return false;
    if (!CopyOneFile(content / ProjectManager::kProjectFileName,
                     dest / ProjectManager::kProjectFileName, errorOut))
        return false;
    if (!CopyDirectoryRecursive(content / "rom", dest / "rom", errorOut))
        return false;

    // Content rom/shader is often a stale snapshot. The host loads .cso from
    // dest/rom/shader at boot, so overlay the shaders just compiled with this
    // IvyEngineRuntime or the engine repo will skin with the wrong Bind.
    const fs::path engineShaders = engineOut / "rom" / "shader";
    const fs::path destShaders = dest / "rom" / "shader";
    if (fs::exists(engineShaders, ec)) {
        if (!CopyDirectoryRecursive(engineShaders, destShaders, errorOut))
            return false;
    }

    // Engine branding is not part of content projects by default — overlay it
    // so the runtime boot screen can find rom/branding/ivy_logo.png.
    const fs::path engineBranding = engineOut / "rom" / "branding";
    const fs::path destBranding = dest / "rom" / "branding";
    if (fs::exists(engineBranding, ec)) {
        if (!CopyDirectoryRecursive(engineBranding, destBranding, errorOut))
            return false;
    }

    const fs::path scriptsBin = dest / "game" / "scripts" / "bin";
    if (!CopyOneFile(content / "game" / "scripts" / "bin" / "GameScripts.dll",
                     scriptsBin / "GameScripts.dll", errorOut))
        return false;

    const fs::path pdbFrom = content / "game" / "scripts" / "bin" / "GameScripts.pdb";
    if (fs::exists(pdbFrom, ec)) {
        if (!CopyOneFile(pdbFrom, scriptsBin / "GameScripts.pdb", errorOut))
            return false;
    }

    return true;
}

DWORD MsBuildProject(const std::string& msbuild, const fs::path& vcxproj,
                     const std::string& configName) {
    const std::wstring cmd =
        L"\"" + WideFromUtf8(msbuild) + L"\" \"" + vcxproj.wstring() +
        L"\" /p:Configuration=" + WideFromUtf8(configName) +
        L" /p:Platform=x64 /v:m";
    const fs::path workDir = vcxproj.parent_path();
    AppendLogLine("MSBuild " + vcxproj.filename().string() + " | " + configName + " | x64");
    return RunProcess(cmd, workDir.wstring());
}

void RunBuild(Request request) {
    request.contentRoot = TrimCopy(request.contentRoot);
    request.destination = TrimCopy(request.destination);

    const std::string configName = ConfigurationName(request.configuration);
    bool succeeded = false;
    std::string result;

    auto finish = [&]() {
        {
            std::lock_guard<std::mutex> lock(g_job.mutex);
            g_job.resultMessage = result;
        }
        g_job.succeeded = succeeded;
        g_job.finished = true;
        g_job.running = false;
    };

    const std::string refusal = DestinationRefusalReason(request.contentRoot, request.destination);
    if (!refusal.empty()) {
        AppendLogLine(refusal);
        result = refusal;
        finish();
        return;
    }

    const std::string engineRootStr = ProjectManager::FindEngineRepoRoot();
    if (engineRootStr.empty()) {
        result = "Engine repo not found. Run the editor from the engine build output.";
        AppendLogLine(result);
        finish();
        return;
    }
    const fs::path engineRoot = engineRootStr;
    AppendLogLine("Engine repo: " + engineRoot.string());

    const fs::path runtimeProj = engineRoot / "IvyEngineRuntime.vcxproj";
    const fs::path scriptsProj = fs::path(request.contentRoot) / "game" / "scripts" /
                                 "GameScripts" / "GameScripts.vcxproj";
    if (!fs::exists(runtimeProj)) {
        result = "Missing IvyEngineRuntime.vcxproj in the engine repo.";
        AppendLogLine(result);
        finish();
        return;
    }
    if (!fs::exists(scriptsProj)) {
        result = "Missing game/scripts/GameScripts/GameScripts.vcxproj in the content project.";
        AppendLogLine(result);
        finish();
        return;
    }

    const std::string msbuild = FindMsBuild();
    if (msbuild.empty()) {
        result = "MSBuild not found. Install Visual Studio with the MSBuild component.";
        AppendLogLine(result);
        finish();
        return;
    }
    AppendLogLine("MSBuild: " + msbuild);

    const DWORD runtimeCode = MsBuildProject(msbuild, runtimeProj, configName);
    if (runtimeCode != 0) {
        result = "IvyEngineRuntime MSBuild failed (exit " + std::to_string(runtimeCode) + ").";
        AppendLogLine(result);
        finish();
        return;
    }

    const DWORD scriptsCode = MsBuildProject(msbuild, scriptsProj, configName);
    if (scriptsCode != 0) {
        result = "GameScripts MSBuild failed (exit " + std::to_string(scriptsCode) + ").";
        AppendLogLine(result);
        finish();
        return;
    }

    AppendLogLine("Assembling playable package...");
    std::string assembleError;
    if (!AssemblePackage(request, engineRoot, configName, &assembleError)) {
        result = assembleError.empty() ? "Assemble failed." : assembleError;
        AppendLogLine(result);
        finish();
        return;
    }

    const std::string exeFile = SanitizeGameExeStem(request.gameName) + ".exe";
    succeeded = true;
    result = "Assembled " + exeFile + " → " + request.destination;
    AppendLogLine(result);
    finish();
}

}  // namespace

const char* ConfigurationName(Configuration configuration) {
    return configuration == Configuration::Release ? "Release" : "Debug";
}

bool TryParseConfiguration(const std::string& name, Configuration* out) {
    if (!out)
        return false;
    if (name == "Release") {
        *out = Configuration::Release;
        return true;
    }
    if (name == "Debug") {
        *out = Configuration::Debug;
        return true;
    }
    return false;
}

std::string DefaultDestination(const std::string& contentRoot) {
    return (fs::path(contentRoot) / "deploy").string();
}

void LoadPrefs(const std::string& contentRoot, Prefs* out) {
    if (!out)
        return;
    out->destination = DefaultDestination(contentRoot);
    out->configuration = Configuration::Debug;

    try {
        std::ifstream file(PrefsFilePath());
        if (!file.is_open())
            return;
        json j;
        file >> j;

        Configuration lastConfig = Configuration::Debug;
        TryParseConfiguration(j.value("lastConfiguration", "Debug"), &lastConfig);
        out->configuration = lastConfig;

        const std::string key = PrefsKey(contentRoot);
        if (!j.contains("projects") || !j["projects"].is_object() || !j["projects"].contains(key))
            return;
        const json& entry = j["projects"][key];
        const std::string dest = entry.value("destination", "");
        if (!dest.empty())
            out->destination = dest;
        TryParseConfiguration(entry.value("configuration", "Debug"), &out->configuration);
    }
    catch (...) {
    }
}

void SavePrefs(const std::string& contentRoot, const Prefs& prefs) {
    json j;
    try {
        std::ifstream file(PrefsFilePath());
        if (file.is_open())
            file >> j;
    }
    catch (...) {
        j = json::object();
    }

    if (!j.is_object())
        j = json::object();
    if (!j.contains("projects") || !j["projects"].is_object())
        j["projects"] = json::object();

    const std::string key = PrefsKey(contentRoot);
    j["lastConfiguration"] = ConfigurationName(prefs.configuration);
    j["projects"][key]["destination"] = prefs.destination;
    j["projects"][key]["configuration"] = ConfigurationName(prefs.configuration);

    try {
        const fs::path path = PrefsFilePath();
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        std::ofstream file(path);
        if (file.is_open())
            file << j.dump(2);
    }
    catch (...) {
    }
}

std::string DestinationRefusalReason(const std::string& contentRoot,
                                     const std::string& destination) {
    const std::string dest = TrimCopy(destination);
    if (dest.empty())
        return "Choose an output folder.";
    if (contentRoot.empty())
        return "No content project is open.";

    const fs::path destPath = dest;
    const fs::path root = contentRoot;
    if (PathsEqual(destPath, root))
        return "Destination cannot be the content project root.";
    if (IsUnderOrEqual(destPath, root / "rom"))
        return "Destination cannot be inside the content project's rom folder.";
    if (IsUnderOrEqual(destPath, root / "game"))
        return "Destination cannot be inside the content project's game folder.";

    const std::string exeDir = ProjectManager::GetExecutableDirectory();
    if (!exeDir.empty() && PathsEqual(destPath, exeDir))
        return "Destination cannot be the folder the editor is running from.";

    return {};
}

bool IsBuildRunning() {
    return g_job.running.load();
}

bool StartBuild(const Request& request) {
    bool expected = false;
    if (!g_job.running.compare_exchange_strong(expected, true))
        return false;

    {
        std::lock_guard<std::mutex> lock(g_job.mutex);
        g_job.log.clear();
        g_job.resultMessage.clear();
    }
    g_job.finished = false;
    g_job.succeeded = false;

    std::thread(RunBuild, request).detach();
    return true;
}

void CopyLog(std::string* out) {
    if (!out)
        return;
    std::lock_guard<std::mutex> lock(g_job.mutex);
    *out = g_job.log;
}

bool IsBuildFinished() {
    return g_job.finished.load();
}

bool LastBuildSucceeded() {
    return g_job.succeeded.load();
}

std::string LastBuildResultMessage() {
    std::lock_guard<std::mutex> lock(g_job.mutex);
    return g_job.resultMessage;
}

}  // namespace PlayablePackage
