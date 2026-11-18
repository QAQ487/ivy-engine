// playable_package_builder.cpp
// Builds playable runtime packages from a project.

#include "playable_package_builder.h"
#include "engine/rendering/environment/panorama_sky.h"
#include "engine/editor/texture_cook.h"
#include "engine/editor/package_assets.h"

#include "engine/core/content_path.h"
#include "project_manager.h"

#include "libraries/nlohmann/json.hpp"

#include <Windows.h>
#include <shlobj.h>

#include <atomic>
#include <cctype>
#include <cstdio>
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
        const fs::path rel = content_path::LexicalChildPath(from, src);
        if (rel.empty()) {
            if (errorOut)
                *errorOut = "Refused to copy a path that leaves the source directory: " + from.string();
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

void CollectRomReferences(const std::string& text, std::vector<std::string>& out) {
    for (std::size_t i = 0; i + 4 < text.size(); ++i) {
        if (text.compare(i, 4, "rom/") != 0)
            continue;
        if (i > 0) {
            const unsigned char prev = static_cast<unsigned char>(text[i - 1]);
            if (std::isalnum(prev) || prev == '_' || prev == '/')
                continue;
        }
        std::size_t end = i;
        while (end < text.size()) {
            const unsigned char c = static_cast<unsigned char>(text[end]);
            if (std::isalnum(c) || c == '/' || c == '_' || c == '.' || c == '-' || c == '+' || c == ' ') {
                ++end;
                continue;
            }
            break;
        }
        std::string token = text.substr(i, end - i);
        while (!token.empty() && (token.back() == ' ' || token.back() == '.'))
            token.pop_back();
        std::string normalized;
        if (content_path::IsSafeRomPath(token, &normalized))
            out.push_back(std::move(normalized));
        i = end;
    }
}

// Keep in step with builtin_assets::TextureEntries and tools/materialize_engine_defaults.py.
const char* const kBuiltinRomPaths[] = {
    "rom/branding/ivy_logo.png",
    "rom/skybox/ibl/brdf_lut.png",
};

bool CopyResourceIfMissing(const fs::path& resources, const fs::path& destRom,
                           const std::string& romPath, std::string* errorOut) {
    std::string normalized;
    if (!content_path::IsSafeRomPath(romPath, &normalized))
        return true;
    const std::string rest = normalized.substr(4);
    if (rest == "skybox/source" || rest.rfind("skybox/source/", 0) == 0)
        return true;
    // Compiled into the exe: content may name these paths, but no file ships.
    for (const char* builtin : kBuiltinRomPaths) {
        if (normalized == builtin)
            return true;
    }
    const fs::path dest = destRom / fs::path(rest);
    std::error_code ec;
    if (fs::is_regular_file(dest, ec))
        return true;
    const fs::path source = resources / fs::path(rest);
    if (!fs::is_regular_file(source, ec))
        return true;
    fs::create_directories(dest.parent_path(), ec);
    fs::copy_file(source, dest, fs::copy_options::skip_existing, ec);
    if (ec) {
        if (errorOut)
            *errorOut = "Failed to copy engine default " + normalized + ": " + ec.message();
        return false;
    }
    return true;
}

bool MaterializeReferencedDefaults(const fs::path& resources, const fs::path& destRom,
                                   std::string* errorOut) {
    static const char* kHostRomPaths[] = {
        "rom/font/LemonSlice.ttf",
    };
    std::vector<std::string> refs(std::begin(kHostRomPaths), std::end(kHostRomPaths));
    std::error_code ec;
    if (fs::is_directory(destRom, ec)) {
        for (fs::recursive_directory_iterator it(destRom, ec), end; it != end; it.increment(ec)) {
            if (ec || !it->is_regular_file(ec))
                continue;
            const std::string ext = it->path().extension().string();
            if (ext != ".json" && ext != ".txt" && ext != ".prefab" && ext != ".physmat" &&
                ext != ".mtl" && ext != ".obj")
                continue;
            std::ifstream file(it->path(), std::ios::binary);
            if (!file)
                continue;
            std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            for (char& ch : text) {
                if (ch == '\\')
                    ch = '/';
            }
            CollectRomReferences(text, refs);
        }
    }
    for (const std::string& romPath : refs) {
        if (!CopyResourceIfMissing(resources, destRom, romPath, errorOut))
            return false;
    }
    return true;
}

struct AssetPackSettings {
    bool packAll = false;
    std::vector<std::string> alwaysInclude;
    std::string defaultScene;
};

AssetPackSettings ReadAssetPackSettings(const fs::path& contentRoot) {
    AssetPackSettings settings;
    try {
        std::ifstream file(contentRoot / ProjectManager::kProjectFileName);
        if (!file)
            return settings;
        json j;
        file >> j;
        settings.defaultScene = j.value("defaultScene", std::string{});
        if (j.contains("build") && j["build"].is_object()) {
            const auto& b = j["build"];
            settings.packAll = b.value("packAllAssets", false);
            if (b.contains("alwaysInclude") && b["alwaysInclude"].is_array()) {
                for (const auto& item : b["alwaysInclude"]) {
                    if (item.is_string())
                        settings.alwaysInclude.push_back(item.get<std::string>());
                }
            }
        }
    }
    catch (const std::exception&) {
        // A broken project.json keeps the defaults; the package then still builds.
    }
    return settings;
}

std::string FormatMegabytes(unsigned long long bytes) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    return buf;
}

// Copies only the rom/ files the default scene reaches (engine/editor/package_assets.h).
bool CopyReferencedRom(const fs::path& content, const fs::path& destRom,
                       const AssetPackSettings& settings, std::string* errorOut) {
    package_assets::Options options;
    options.romRoot = content / "rom";
    options.rootScenes = {settings.defaultScene};
    options.alwaysInclude = settings.alwaysInclude;
    options.scanFiles = {content / ProjectManager::kProjectFileName,
                         content / "game" / "scripts" / "bin" / "GameScripts.dll"};
    const package_assets::Result result = package_assets::ComputeReferencedAssets(options);

    std::error_code ec;
    fs::create_directories(destRom, ec);
    for (const std::string& rel : result.included) {
        const fs::path from = content / "rom" / PathFromUtf8(rel);
        const fs::path to = destRom / PathFromUtf8(rel);
        fs::create_directories(to.parent_path(), ec);
        fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            if (errorOut)
                *errorOut = "Failed to copy " + from.string() + ": " + ec.message();
            return false;
        }
    }

    for (const std::string& warning : result.warnings)
        AppendLogLine("Warning: " + warning);
    AppendLogLine("Packing referenced rom/ assets: " + std::to_string(result.included.size()) + " files (" +
                  FormatMegabytes(result.includedBytes) + "); left out " +
                  std::to_string(result.excluded.size()) + " unreferenced files (" +
                  FormatMegabytes(result.excludedBytes) + ").");
    constexpr std::size_t kMaxListed = 80;
    for (std::size_t i = 0; i < result.excluded.size() && i < kMaxListed; ++i)
        AppendLogLine("  skip rom/" + result.excluded[i]);
    if (result.excluded.size() > kMaxListed)
        AppendLogLine("  ... and " + std::to_string(result.excluded.size() - kMaxListed) + " more");
    if (!result.excluded.empty())
        AppendLogLine("If the game loads a left-out file by a name it builds at runtime, add it to "
                      "project.json build.alwaysInclude, or set build.packAllAssets to true.");
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
    // The debug text font is compiled into the exe (engine/core/builtin_assets.h); OFL
    // asks for its license to travel with it.
    if (!CopyOneFile(engineRoot / "resources" / "font" / "OFL-IBMPlexMono.txt",
                     dest / "licenses" / "IBMPlexMono-OFL.txt", errorOut))
        return false;
    if (!CopyOneFile(content / ProjectManager::kProjectFileName,
                     dest / ProjectManager::kProjectFileName, errorOut))
        return false;

    // Cook panorama PNGs into the project rom before the copy so the package
    // carries the DDS. The runtime re-cooks only when the stamp does not match.
    std::string panoramaError;
    if (!panorama::CookProjectPanoramas(content, engineRoot / "resources", &panoramaError)) {
        if (errorOut)
            *errorOut = panoramaError.empty() ? "Panorama skybox cook failed." : panoramaError;
        return false;
    }

    const AssetPackSettings packSettings = ReadAssetPackSettings(content);
    if (packSettings.packAll) {
        AppendLogLine("Packing all of rom/ (project.json build.packAllAssets).");
        if (!CopyDirectoryRecursive(content / "rom", dest / "rom", errorOut))
            return false;
    }
    else if (!CopyReferencedRom(content, dest / "rom", packSettings, errorOut)) {
        return false;
    }

    // Compress the packaged copy (never the content project) to BC7 .dds and drop the
    // source images. The runtime falls back from a missing .png/.jpg/.bmp to its .dds.
    {
        std::string textureError;
        if (!texture_cook::CookPackageTextures(dest / "rom", AppendLogLine, nullptr, &textureError)) {
            if (errorOut)
                *errorOut = textureError.empty() ? "Texture cook failed." : textureError;
            return false;
        }
    }

    // The host loads .cso from dest/rom/shader at boot. Overlay the shaders
    // compiled with this IvyEngineRuntime. Prefer the OutDir mirror, then the
    // repo resources tree written by the shader target.
    fs::path engineShaders = engineOut / "resources" / "shader";
    if (!fs::is_directory(engineShaders, ec))
        engineShaders = engineRoot / "resources" / "shader";
    const fs::path destShaders = dest / "rom" / "shader";
    if (fs::is_directory(engineShaders, ec)) {
        if (!CopyDirectoryRecursive(engineShaders, destShaders, errorOut))
            return false;
    }

    // The boot-screen logo and the PBR BRDF LUT are compiled into the exe
    // (engine/core/builtin_assets.h), so nothing from resources/branding is copied.

    // Keep this list aligned with tools/materialize_engine_defaults.py.
    // MySkybox.dds is not in it: that file is copied only when content names it.
    if (!MaterializeReferencedDefaults(engineRoot / "resources", dest / "rom", errorOut))
        return false;

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
