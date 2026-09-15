#include "project_manager.h"

#include "engine/audio/audio_manager.h"
#include "engine/physics/core/rigidbody.h"
#include "libraries/nlohmann/json.hpp"

#include <Windows.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <system_error>
#include <thread>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

constexpr size_t kMaxRecentProjects = 12;

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
    }
    return true;
}

std::string ReadDefaultSceneFileName(const fs::path& root) {
    const fs::path cfg = root / "rom" / "scene" / "default_scene.txt";
    std::ifstream file(cfg);
    if (!file.is_open())
        return {};
    std::string line;
    std::getline(file, line);
    const char* ws = " \t\r\n";
    const size_t begin = line.find_first_not_of(ws);
    if (begin == std::string::npos)
        return {};
    const size_t end = line.find_last_not_of(ws);
    return line.substr(begin, end - begin + 1);
}

void WriteDefaultSceneFile(const fs::path& root, const std::string& sceneFile) {
    if (sceneFile.empty())
        return;
    std::error_code ec;
    fs::create_directories(root / "rom" / "scene", ec);
    std::ofstream file(root / "rom" / "scene" / "default_scene.txt", std::ios::trunc);
    if (file.is_open())
        file << sceneFile << '\n';
}

std::string SanitizeProjectFolderName(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' ||
            c == '|' || c == '?' || c == '*') {
            out.push_back('_');
        }
        else {
            out.push_back(c);
        }
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
        out.pop_back();
    return out.empty() ? "NewProject" : out;
}

// Must match external_content_template/.../GameScripts.vcxproj ProjectGuid.
constexpr const char* kGameScriptsProjectGuid = "{E9E3B9BC-BAD1-4862-AE40-2358478F1C8A}";

bool WriteContentProjectSolution(const fs::path& root, std::string* errorOut) {
    const std::string folderName = root.filename().string();
    if (folderName.empty() || folderName == "." || folderName == "..") {
        if (errorOut)
            *errorOut = "Cannot derive .sln name from project folder";
        return false;
    }

    const fs::path slnPath = root / (folderName + ".sln");
    std::ofstream sln(slnPath, std::ios::trunc);
    if (!sln.is_open()) {
        if (errorOut)
            *errorOut = "Failed to create " + slnPath.string();
        return false;
    }

    sln <<
        "Microsoft Visual Studio Solution File, Format Version 12.00\n"
        "# Visual Studio Version 17\n"
        "VisualStudioVersion = 17.0.31903.59\n"
        "MinimumVisualStudioVersion = 10.0.40219.1\n"
        "Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"GameScripts\", "
        "\"game\\scripts\\GameScripts\\GameScripts.vcxproj\", \""
        << kGameScriptsProjectGuid << "\"\n"
        "EndProject\n"
        "Global\n"
        "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n"
        "\t\tDebug|x64 = Debug|x64\n"
        "\t\tDebug|x86 = Debug|x86\n"
        "\t\tRelease|x64 = Release|x64\n"
        "\t\tRelease|x86 = Release|x86\n"
        "\tEndGlobalSection\n"
        "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n"
        "\t\t" << kGameScriptsProjectGuid << ".Debug|x64.ActiveCfg = Debug|x64\n"
        "\t\t" << kGameScriptsProjectGuid << ".Debug|x64.Build.0 = Debug|x64\n"
        "\t\t" << kGameScriptsProjectGuid << ".Debug|x86.ActiveCfg = Debug|Win32\n"
        "\t\t" << kGameScriptsProjectGuid << ".Debug|x86.Build.0 = Debug|Win32\n"
        "\t\t" << kGameScriptsProjectGuid << ".Release|x64.ActiveCfg = Release|x64\n"
        "\t\t" << kGameScriptsProjectGuid << ".Release|x64.Build.0 = Release|x64\n"
        "\t\t" << kGameScriptsProjectGuid << ".Release|x86.ActiveCfg = Release|Win32\n"
        "\t\t" << kGameScriptsProjectGuid << ".Release|x86.Build.0 = Release|Win32\n"
        "\tEndGlobalSection\n"
        "\tGlobalSection(SolutionProperties) = preSolution\n"
        "\t\tHideSolutionNode = FALSE\n"
        "\tEndGlobalSection\n"
        "EndGlobal\n";
    return true;
}

bool WriteContentBuildScriptsBat(const fs::path& root, std::string* errorOut) {
    const fs::path batPath = root / "build_scripts.bat";
    std::ofstream bat(batPath, std::ios::trunc);
    if (!bat.is_open()) {
        if (errorOut)
            *errorOut = "Failed to create " + batPath.string();
        return false;
    }

    bat <<
        "@echo off\n"
        "setlocal\n"
        "set ROOT=%~dp0\n"
        "set CONFIG=Debug\n"
        "if not \"%1\"==\"\" set CONFIG=%1\n"
        "\n"
        "set MSBUILD=\n"
        "for /f \"usebackq delims=\" %%i in (`\"%ProgramFiles(x86)%\\Microsoft Visual Studio\\Installer\\vswhere.exe\" "
        "-latest -requires Microsoft.Component.MSBuild -find MSBuild\\**\\Bin\\MSBuild.exe`) do set MSBUILD=%%i\n"
        "\n"
        "if \"%MSBUILD%\"==\"\" (\n"
        "  echo MSBuild not found. Open the project .sln in Visual Studio instead.\n"
        "  exit /b 1\n"
        ")\n"
        "\n"
        "echo Building GameScripts ^| %CONFIG% ^| x64\n"
        "\"%MSBUILD%\" \"%ROOT%game\\scripts\\GameScripts\\GameScripts.vcxproj\" "
        "/p:Configuration=%CONFIG% /p:Platform=x64 /v:m\n"
        "exit /b %ERRORLEVEL%\n";
    return true;
}

const char* kBlankSceneJson = R"({
    "version": 4,
    "settings": {
        "render": {
            "skyboxEnabled": false,
            "skyboxCubemap": "",
            "skyboxTint": { "r": 1.0, "g": 1.0, "b": 1.0, "a": 1.0 },
            "skyboxExposure": 1.0,
            "skyboxRotationYDeg": 0.0,
            "clearColor": { "r": 0.12, "g": 0.14, "b": 0.18, "a": 1.0 },
            "defaultShader": "standard",
            "shadowQuality": "medium"
        }
    },
    "nodes": [
        {
            "name": "Directional Light",
            "tag": 0,
            "components": {
                "SceneTransform": {
                    "posX": 0.0, "posY": 8.0, "posZ": 0.0,
                    "rotX": 50.0, "rotY": -35.0, "rotZ": 0.0,
                    "scaleX": 1.0, "scaleY": 1.0, "scaleZ": 1.0
                },
                "DirectionalLight": {
                    "enabled": true,
                    "castShadows": true,
                    "color": [1.0, 0.96, 0.9],
                    "intensity": 1.0,
                    "specColor": [1.0, 1.0, 1.0],
                    "specPower": 32.0
                }
            },
            "children": []
        }
    ],
    "canvas": {
        "name": "main",
        "elements": []
    }
}
)";

} // namespace

ProjectManager& ProjectManager::Instance() {
    static ProjectManager instance;
    return instance;
}

std::string ProjectManager::Utf8FromWide(const std::wstring& wide) {
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

std::wstring ProjectManager::WideFromUtf8(const std::string& utf8) {
    if (utf8.empty())
        return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(),
                                         nullptr, 0);
    if (size <= 0)
        return {};
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), out.data(), size);
    return out;
}

std::string ProjectManager::GetExecutableDirectory() {
    wchar_t buf[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
        return {};
    return Utf8FromWide(fs::path(buf).parent_path().wstring());
}

std::string ProjectManager::FindEngineRepoRoot() {
    // Walk up from cwd and exe dir looking for the external content template.
    std::vector<fs::path> seeds;
    seeds.push_back(fs::current_path());
    seeds.push_back(fs::path(GetExecutableDirectory()));

    for (const fs::path& seed : seeds) {
        fs::path cur = seed;
        for (int i = 0; i < 8; ++i) {
            if (fs::exists(cur / "external_content_template") &&
                fs::exists(cur / "Game_Project.sln")) {
                return cur.string();
            }
            if (!cur.has_parent_path() || cur == cur.parent_path())
                break;
            cur = cur.parent_path();
        }
    }
    return {};
}

std::string ProjectManager::RecentListPath() {
    wchar_t* appData = nullptr;
    std::string path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
        path = Utf8FromWide(appData);
        CoTaskMemFree(appData);
        path += "/GameEngine/recent_projects.json";
        return path;
    }
    return GetExecutableDirectory() + "/recent_projects.json";
}

void ProjectManager::LoadRecentList() {
    m_recent.clear();
    try {
        std::ifstream file(RecentListPath());
        if (!file.is_open())
            return;
        json j;
        file >> j;
        if (!j.contains("projects") || !j["projects"].is_array())
            return;
        for (const auto& entry : j["projects"]) {
            if (!entry.is_string())
                continue;
            const std::string p = entry.get<std::string>();
            if (!p.empty())
                m_recent.push_back(p);
        }
    }
    catch (...) {
        m_recent.clear();
    }
}

void ProjectManager::SaveRecentList() const {
    try {
        const fs::path path = RecentListPath();
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        json j;
        j["projects"] = m_recent;
        std::ofstream file(path, std::ios::trunc);
        if (file.is_open())
            file << j.dump(2);
    }
    catch (...) {
    }
}

void ProjectManager::TouchRecent(const std::string& rootPath) {
    if (rootPath.empty())
        return;
    std::error_code ec;
    const std::string canonical = fs::weakly_canonical(rootPath, ec).string();
    const std::string key = ec ? rootPath : canonical;

    m_recent.erase(std::remove(m_recent.begin(), m_recent.end(), key), m_recent.end());
    // Also remove non-canonical duplicates that match after weakly_canonical.
    m_recent.erase(
        std::remove_if(m_recent.begin(), m_recent.end(),
                       [&](const std::string& existing) {
                           std::error_code ec2;
                           const std::string c = fs::weakly_canonical(existing, ec2).string();
                           return (!ec2 && c == key) || existing == rootPath;
                       }),
        m_recent.end());

    m_recent.insert(m_recent.begin(), key);
    if (m_recent.size() > kMaxRecentProjects)
        m_recent.resize(kMaxRecentProjects);
    SaveRecentList();
}

ProjectManager::ProjectData ProjectManager::MakeDefaultData(const std::string& rootPath,
                                                           const std::string& name) const {
    ProjectData data;
    data.name = name.empty() ? fs::path(rootPath).filename().string() : name;
    if (data.name.empty())
        data.name = "Untitled";
    data.version = kSchemaVersion;

    const std::string fromTxt = ReadDefaultSceneFileName(rootPath);
    data.defaultScene = fromTxt.empty() ? "scene_main.json" : fromTxt;
    data.lastOpenedScene = data.defaultScene;
    data.editor.uiZoomPercent = 100;
    data.physics.gravity = GRAVITY;

    try {
        std::ifstream prefs(fs::path(rootPath) / "rom" / "editor" / "ui_prefs.json");
        if (prefs.is_open()) {
            json j;
            prefs >> j;
            data.editor.uiZoomPercent = j.value("uiZoomPercent", 100);
        }
    }
    catch (...) {
    }
    return data;
}

bool ProjectManager::LoadProjectFile(const std::string& rootPath, ProjectData* out,
                                     std::string* errorOut) {
    if (!out)
        return false;
    const fs::path filePath = fs::path(rootPath) / kProjectFileName;
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            if (errorOut)
                *errorOut = "Cannot open " + filePath.string();
            return false;
        }
        json j;
        file >> j;
        out->name = j.value("name", fs::path(rootPath).filename().string());
        out->version = j.value("version", kSchemaVersion);
        out->lastOpenedScene = j.value("lastOpenedScene", std::string{});
        out->defaultScene = j.value("defaultScene", std::string{});
        out->editor.uiZoomPercent = 100;
        if (j.contains("editor") && j["editor"].is_object())
            out->editor.uiZoomPercent = j["editor"].value("uiZoomPercent", 100);
        out->physics.gravity = GRAVITY;
        if (j.contains("physics") && j["physics"].is_object())
            out->physics.gravity = j["physics"].value("gravity", GRAVITY);
        if (!(out->physics.gravity > 0.0f))
            out->physics.gravity = GRAVITY;

        out->audio = AudioSettings{};
        if (j.contains("audio") && j["audio"].is_object()) {
            const auto& a = j["audio"];
            out->audio.masterVolume = a.value("masterVolume", 1.0f);
            out->audio.bgmVolume = a.value("bgmVolume", 1.0f);
            out->audio.sfxVolume = a.value("sfxVolume", 1.0f);
            out->audio.voiceVolume = a.value("voiceVolume", 1.0f);
            out->audio.uiClickSound = a.value("uiClickSound", std::string{});
        }

        if (out->defaultScene.empty())
            out->defaultScene = ReadDefaultSceneFileName(rootPath);

        // Prefer a scene that actually exists. Stale project.json entries used to
        // override a valid default_scene.txt and make "Set Default Scene" look broken.
        auto sceneExists = [&](const std::string& fileName) -> bool {
            if (fileName.empty())
                return false;
            return fs::exists(fs::path(rootPath) / "rom" / "scene" / fileName);
        };
        if (!sceneExists(out->defaultScene)) {
            const std::string fromTxt = ReadDefaultSceneFileName(rootPath);
            if (sceneExists(fromTxt))
                out->defaultScene = fromTxt;
        }

        if (out->lastOpenedScene.empty() || !sceneExists(out->lastOpenedScene))
            out->lastOpenedScene = out->defaultScene;
        return true;
    }
    catch (const std::exception& ex) {
        if (errorOut)
            *errorOut = ex.what();
        return false;
    }
}

bool ProjectManager::WriteProjectFile(const std::string& rootPath, const ProjectData& data,
                                      std::string* errorOut) const {
    try {
        json j;
        j["name"] = data.name;
        j["version"] = data.version;
        j["lastOpenedScene"] = data.lastOpenedScene;
        j["defaultScene"] = data.defaultScene;
        j["editor"] = json::object();
        j["editor"]["uiZoomPercent"] = data.editor.uiZoomPercent;
        j["physics"] = json::object();
        j["physics"]["gravity"] = data.physics.gravity;
        j["audio"] = json::object();
        j["audio"]["masterVolume"] = data.audio.masterVolume;
        j["audio"]["bgmVolume"] = data.audio.bgmVolume;
        j["audio"]["sfxVolume"] = data.audio.sfxVolume;
        j["audio"]["voiceVolume"] = data.audio.voiceVolume;
        j["audio"]["uiClickSound"] = data.audio.uiClickSound;

        const fs::path filePath = fs::path(rootPath) / kProjectFileName;
        std::ofstream file(filePath, std::ios::trunc);
        if (!file.is_open()) {
            if (errorOut)
                *errorOut = "Cannot write " + filePath.string();
            return false;
        }
        file << j.dump(2);
        WriteDefaultSceneFile(rootPath, data.defaultScene);
        return true;
    }
    catch (const std::exception& ex) {
        if (errorOut)
            *errorOut = ex.what();
        return false;
    }
}

void ProjectManager::EnsureEngineAssets(const std::string& rootPath) const {
    const fs::path exeRom = fs::path(GetExecutableDirectory()) / "rom";
    const fs::path projectRom = fs::path(rootPath) / "rom";
    std::error_code ec;

    // Engine builtins required after SetCurrentDirectory(projectRoot).
    // Shaders always overlay: a non-empty rom/shader is often a stale .cso
    // snapshot, and the host binds those at boot.
    const char* fillIfEmpty[] = { "font", "texture" };
    for (const char* folder : fillIfEmpty) {
        const fs::path src = exeRom / folder;
        const fs::path dst = projectRom / folder;
        if (!fs::exists(src, ec))
            continue;
        if (fs::exists(dst, ec) && !fs::is_empty(dst, ec))
            continue;
        CopyDirectoryRecursive(src, dst, nullptr);
    }

    const fs::path shaderSrc = exeRom / "shader";
    const fs::path shaderDst = projectRom / "shader";
    if (fs::exists(shaderSrc, ec))
        CopyDirectoryRecursive(shaderSrc, shaderDst, nullptr);

    // Engine branding (logo) — always overlay so playable packages / projects
    // opened before branding existed still pick it up from the host exe rom.
    const fs::path brandingSrc = exeRom / "branding";
    const fs::path brandingDst = projectRom / "branding";
    if (fs::exists(brandingSrc, ec))
        CopyDirectoryRecursive(brandingSrc, brandingDst, nullptr);
}

bool ProjectManager::ActivateRoot(const std::string& rootPath, std::string* errorOut) {
    std::error_code ec;
    const fs::path root = fs::weakly_canonical(rootPath, ec);
    if (ec || !fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        if (errorOut)
            *errorOut = "Project folder does not exist: " + rootPath;
        return false;
    }

    if (!fs::exists(root / "rom", ec)) {
        if (errorOut)
            *errorOut = "Not a valid project (missing rom/): " + root.string();
        return false;
    }

    const std::wstring wideRoot = root.wstring();
    if (!SetCurrentDirectoryW(wideRoot.c_str())) {
        if (errorOut)
            *errorOut = "Failed to set working directory to " + root.string();
        return false;
    }

    EnsureEngineAssets(root.string());

    ProjectData data;
    const fs::path projectFile = root / kProjectFileName;
    if (fs::exists(projectFile, ec)) {
        if (!LoadProjectFile(root.string(), &data, errorOut))
            return false;
    }
    else {
        data = MakeDefaultData(root.string(), root.filename().string());
        if (!WriteProjectFile(root.string(), data, errorOut))
            return false;
    }

    m_rootPath = root.string();
    m_data = std::move(data);
    m_open = true;
    Physics_SetGravity(m_data.physics.gravity);
    AudioManager::Instance().SetMasterVolume(m_data.audio.masterVolume);
    AudioManager::Instance().SetBGMVolume(m_data.audio.bgmVolume);
    AudioManager::Instance().SetSFXVolume(m_data.audio.sfxVolume);
    AudioManager::Instance().SetVoiceVolume(m_data.audio.voiceVolume);
    AudioManager::Instance().SetUiClickSound(m_data.audio.uiClickSound);
    TouchRecent(m_rootPath);
    return true;
}

bool ProjectManager::OpenProject(const std::string& rootPath, std::string* errorOut) {
    return ActivateRoot(rootPath, errorOut);
}

bool ProjectManager::ScaffoldNewProjectTree(const std::string& rootPath,
                                            const std::string& projectName,
                                            std::string* errorOut) {
    std::error_code ec;
    const fs::path root = rootPath;
    fs::create_directories(root / "rom" / "scene", ec);
    fs::create_directories(root / "rom" / "editor", ec);
    fs::create_directories(root / "rom" / "model", ec);
    fs::create_directories(root / "rom" / "texture", ec);
    fs::create_directories(root / "rom" / "prefabs", ec);
    fs::create_directories(root / "game" / "scripts" / "bin", ec);

    {
        std::ofstream scene(root / "rom" / "scene" / "scene_main.json", std::ios::trunc);
        if (!scene.is_open()) {
            if (errorOut)
                *errorOut = "Failed to create scene_main.json";
            return false;
        }
        scene << kBlankSceneJson;
    }
    WriteDefaultSceneFile(root, "scene_main.json");

    EnsureEngineAssets(rootPath);

    // Prefer copying GameScripts template from the engine repo when available.
    const fs::path engineRoot = ProjectManager::FindEngineRepoRoot();
    if (!engineRoot.empty()) {
        const fs::path templateRoot = engineRoot / "external_content_template";
        const fs::path scriptsSrc = templateRoot / "game" / "scripts";
        if (fs::exists(scriptsSrc, ec)) {
            if (!CopyDirectoryRecursive(scriptsSrc, root / "game" / "scripts", errorOut))
                return false;
        }

        // Point EngineRoot at the discovered engine repo.
        const fs::path propsPath = root / "IvyEngine.props";
        std::ofstream props(propsPath, std::ios::trunc);
        if (props.is_open()) {
            props << "<Project>\n"
                     "  <PropertyGroup>\n"
                     "    <EngineRoot Condition=\"'$(EngineRoot)'==''\">"
                  << engineRoot.string()
                  << "\\</EngineRoot>\n"
                     "    <EngineOutDir>$(EngineRoot)x64\\$(Configuration)\\</EngineOutDir>\n"
                     "  </PropertyGroup>\n"
                     "</Project>\n";
        }

        const fs::path deploySrc = templateRoot / "deploy";
        if (fs::exists(deploySrc, ec))
            CopyDirectoryRecursive(deploySrc, root / "deploy", nullptr);
    }
    else {
        fs::create_directories(root / "game" / "scripts" / "GameScripts", ec);
    }

    // Always emit a content-project .sln + build helper so scripts build outside
    // the engine repo without opening Game_Project.sln.
    if (!WriteContentProjectSolution(root, errorOut))
        return false;
    if (!WriteContentBuildScriptsBat(root, errorOut))
        return false;

    ProjectData data = MakeDefaultData(rootPath, projectName);
    data.lastOpenedScene = "scene_main.json";
    data.defaultScene = "scene_main.json";
    return WriteProjectFile(rootPath, data, errorOut);
}

bool ProjectManager::CreateProject(const std::string& parentDir, const std::string& projectName,
                                   std::string* errorOut) {
    const std::string folderName = SanitizeProjectFolderName(projectName);
    if (parentDir.empty()) {
        if (errorOut)
            *errorOut = "Parent folder is empty";
        return false;
    }

    std::error_code ec;
    const fs::path root = fs::path(parentDir) / folderName;
    if (fs::exists(root, ec)) {
        if (errorOut)
            *errorOut = "Folder already exists: " + root.string();
        return false;
    }

    fs::create_directories(root, ec);
    if (ec) {
        if (errorOut)
            *errorOut = "Failed to create folder: " + ec.message();
        return false;
    }

    if (!ScaffoldNewProjectTree(root.string(), projectName.empty() ? folderName : projectName,
                                errorOut)) {
        return false;
    }
    return ActivateRoot(root.string(), errorOut);
}

bool ProjectManager::SaveProjectFile(std::string* errorOut) {
    if (!m_open) {
        if (errorOut)
            *errorOut = "No project is open";
        return false;
    }
    return WriteProjectFile(m_rootPath, m_data, errorOut);
}

void ProjectManager::SetLastOpenedScene(const std::string& fileName) {
    m_data.lastOpenedScene = fileName;
}

void ProjectManager::SetDefaultScene(const std::string& fileName) {
    m_data.defaultScene = fileName;
}

void ProjectManager::SetEditorUiZoomPercent(int percent) {
    m_data.editor.uiZoomPercent = percent;
}

void ProjectManager::SetPhysicsGravity(float gravity) {
    m_data.physics.gravity = (gravity > 0.0f) ? gravity : GRAVITY;
    if (m_open)
        Physics_SetGravity(m_data.physics.gravity);
}

void ProjectManager::SetAudioBuses(float master, float bgm, float sfx, float voice) {
    m_data.audio.masterVolume = master;
    m_data.audio.bgmVolume = bgm;
    m_data.audio.sfxVolume = sfx;
    m_data.audio.voiceVolume = voice;
}

void ProjectManager::SetUiClickSound(const std::string& path) {
    m_data.audio.uiClickSound = path;
    AudioManager::Instance().SetUiClickSound(path);
}

std::string ProjectManager::ResolveSceneToLoad() const {
    auto makePath = [](const std::string& fileName) -> std::string {
        if (fileName.empty())
            return {};
        if (fileName.find('/') != std::string::npos || fileName.find('\\') != std::string::npos)
            return fileName;
        return std::string("rom/scene/") + fileName;
    };

    if (!m_data.lastOpenedScene.empty()) {
        const std::string path = makePath(m_data.lastOpenedScene);
        if (fs::exists(path))
            return path;
    }
    if (!m_data.defaultScene.empty()) {
        const std::string path = makePath(m_data.defaultScene);
        if (fs::exists(path))
            return path;
    }
    // Fall back to whatever SceneLoader resolved as the default scene path.
    return {};
}

bool ProjectManager::BootstrapAtStartup() {
    if (m_open)
        return true;

    LoadRecentList();

    for (const std::string& recent : m_recent) {
        if (recent.empty())
            continue;
        std::error_code ec;
        if (!fs::exists(fs::path(recent) / "rom", ec))
            continue;
        if (ActivateRoot(recent, nullptr))
            return true;
    }

    return false;
}

void ProjectManager::CloseProject() {
    m_open = false;
    m_rootPath.clear();
    m_data = ProjectData{};
}

void ProjectManager::UpdateWindowTitle(HWND hwnd) const {
    if (!hwnd)
        return;
    std::string title = "Ivy Engine";
    if (m_open) {
        title = m_data.name;
        title += " - ";
        title += m_rootPath;
    }
    SetWindowTextW(hwnd, WideFromUtf8(title).c_str());
}

bool ProjectManager::PickFolder(HWND owner, const wchar_t* title, std::string* outPath,
                                const char* startFolder) {
    if (!outPath)
        return false;
    outPath->clear();

    // owner is unused: dialog runs on an STA worker thread (no cross-thread HWND).
    (void)owner;

    // IFileDialog::Show requires STA and can hang forever on an MTA thread — run it
    // on a short-lived STA worker. Do not pass HWND across threads (use null owner).
    struct DialogResult {
        std::string path;
        HRESULT hrInit = E_FAIL;
    } result;

    const std::wstring titleCopy = title ? std::wstring(title) : std::wstring{};
    const std::wstring startCopy = (startFolder && startFolder[0])
        ? WideFromUtf8(startFolder)
        : std::wstring{};

    std::thread staThread([&result, titleCopy, startCopy]() {
        result.hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const bool needUninit = (result.hrInit == S_OK);
        if (FAILED(result.hrInit) && result.hrInit != RPC_E_CHANGED_MODE) {
            return;
        }

        IFileDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || !dialog)
            return;

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        if (!titleCopy.empty())
            dialog->SetTitle(titleCopy.c_str());

        if (!startCopy.empty()) {
            IShellItem* startItem = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(startCopy.c_str(), nullptr,
                                                      IID_PPV_ARGS(&startItem))) &&
                startItem) {
                dialog->SetFolder(startItem);
                startItem->Release();
            }
        }

        const HRESULT hrShow = dialog->Show(nullptr);
        if (SUCCEEDED(hrShow)) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item) {
                PWSTR pathW = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pathW)) && pathW) {
                    result.path = Utf8FromWide(pathW);
                    CoTaskMemFree(pathW);
                }
                item->Release();
            }
        }

        dialog->Release();
        if (needUninit)
            CoUninitialize();
    });
    staThread.join();

    *outPath = result.path;
    return !outPath->empty();
}
