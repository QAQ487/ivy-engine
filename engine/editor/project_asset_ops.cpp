// project_asset_ops.cpp
// Project asset create/rename/delete operations.

#include "project_asset_ops.h"

#include "project_asset_index.h"
#include "project_manager.h"
#include "engine/ecs/string_utils.h"
#include "engine/scene/scene_loader.h"

#include <Windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <locale>
#include <sstream>
#include <vector>

#pragma comment(lib, "shell32")

namespace fs = std::filesystem;

namespace {

const char* kScriptsPrefix = "Scripts";

std::string ToLowerCopy(std::string value) {
    const std::locale loc = std::locale::classic();
    std::transform(value.begin(), value.end(), value.begin(),
        [loc](unsigned char c) {
            return static_cast<char>(std::tolower(c, loc));
        });
    return value;
}

std::string NormalizeSlashes(std::string path) {
    for (char& c : path) {
        if (c == '\\')
            c = '/';
    }
    while (!path.empty() && path.back() == '/')
        path.pop_back();
    return path;
}

std::string ParentRel(const std::string& relPath) {
    const std::string normalized = NormalizeSlashes(relPath);
    const size_t slash = normalized.find_last_of('/');
    if (slash == std::string::npos)
        return {};
    return normalized.substr(0, slash);
}

std::string LeafName(const std::string& relPath) {
    const std::string normalized = NormalizeSlashes(relPath);
    const size_t slash = normalized.find_last_of('/');
    if (slash == std::string::npos)
        return normalized;
    return normalized.substr(slash + 1);
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

fs::path PathFromUtf8(const std::string& utf8) {
    return fs::path(ToWString(utf8));
}

std::string Utf8FromPath(const fs::path& path) {
    return NormalizeSlashes(Utf8FromWide(path.wstring()));
}

fs::path ProjectRootPath() {
    const std::string& root = ProjectManager::Instance().GetRootPath();
    if (!root.empty())
        return PathFromUtf8(root);
    std::error_code ec;
    fs::path cwd = fs::current_path(ec);
    return ec ? fs::path() : cwd;
}

std::string GenericLower(const fs::path& path) {
    return ToLowerCopy(Utf8FromPath(path));
}

fs::path MakeAbsoluteNormalized(const fs::path& path) {
    std::error_code ec;
    fs::path abs = fs::absolute(path, ec);
    if (ec)
        abs = path;
    abs = abs.lexically_normal();
    return abs;
}

std::string CurrentSceneAbsoluteNormalized() {
    const std::string& current = SceneLoader_GetCurrentScenePath();
    if (current.empty())
        return {};

    fs::path scenePath = PathFromUtf8(NormalizeSlashes(current));
    if (scenePath.is_relative())
        scenePath = ProjectRootPath() / scenePath;
    return GenericLower(MakeAbsoluteNormalized(scenePath));
}

bool RecyclePath(const fs::path& absPath, std::string& error) {
    std::wstring wide = absPath.lexically_normal().wstring();
    for (wchar_t& c : wide) {
        if (c == L'/')
            c = L'\\';
    }
    if (wide.empty()) {
        error = "Invalid path.";
        return false;
    }

    std::vector<wchar_t> from(wide.size() + 2, L'\0');
    std::copy(wide.begin(), wide.end(), from.begin());

    SHFILEOPSTRUCTW op{};
    op.wFunc = FO_DELETE;
    op.pFrom = from.data();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
    const int result = SHFileOperationW(&op);
    if (result == 0 && !op.fAnyOperationsAborted)
        return true;

    std::ostringstream msg;
    msg << "[ProjectAssetOps] SHFileOperationW recycle failed (" << result
        << "), falling back to permanent delete: " << Utf8FromPath(absPath) << "\n";
    OutputDebugStringA(msg.str().c_str());
    error = "Recycle Bin delete failed; permanently deleted instead.";
    return false;
}

} // namespace

bool ProjectAssetOps::IsScriptsPath(const std::string& relPath) {
    const std::string normalized = NormalizeSlashes(relPath);
    if (normalized == kScriptsPrefix)
        return true;
    const size_t prefixLen = std::char_traits<char>::length(kScriptsPrefix);
    return normalized.size() > prefixLen &&
        normalized.compare(0, prefixLen, kScriptsPrefix) == 0 &&
        normalized[prefixLen] == '/';
}

bool ProjectAssetOps::IsValidLeafName(const std::string& name, std::string& error) {
    if (name.empty()) {
        error = "Name cannot be empty.";
        return false;
    }
    if (name.find_first_of("/\\") != std::string::npos) {
        error = "Name cannot contain path separators.";
        return false;
    }
    if (name == "." || name == "..") {
        error = "Invalid name.";
        return false;
    }
    return true;
}

std::string ProjectAssetOps::AssetPathFromRelative(const std::string& relPath) {
    const std::string normalized = NormalizeSlashes(relPath);
    if (IsScriptsPath(normalized)) {
        if (normalized == kScriptsPrefix)
            return "game/scripts";
        return std::string("game/scripts/") + normalized.substr(
            std::char_traits<char>::length(kScriptsPrefix) + 1);
    }
    if (normalized.empty())
        return ProjectAssetIndex::AssetRootPath();
    return std::string(ProjectAssetIndex::AssetRootPath()) + "/" + normalized;
}

std::string ProjectAssetOps::AbsolutePathFromRelative(const std::string& relPath) {
    return Utf8FromPath(MakeAbsoluteNormalized(
        ProjectRootPath() / PathFromUtf8(AssetPathFromRelative(relPath))));
}

bool ProjectAssetOps::IsCurrentSceneOpen(const std::string& relPath) {
    const std::string current = CurrentSceneAbsoluteNormalized();
    if (current.empty() || relPath.empty())
        return false;
    const std::string target = GenericLower(PathFromUtf8(AbsolutePathFromRelative(relPath)));
    return !target.empty() && target == current;
}

bool ProjectAssetOps::FolderContainsCurrentScene(const std::string& relPath) {
    const std::string current = CurrentSceneAbsoluteNormalized();
    if (current.empty())
        return false;

    const std::string folder = GenericLower(PathFromUtf8(AbsolutePathFromRelative(relPath)));
    if (folder.empty())
        return false;
    if (current == folder)
        return true;
    const std::string prefix = folder + "/";
    return current.size() > prefix.size() &&
        current.compare(0, prefix.size(), prefix) == 0;
}

std::vector<std::string> ProjectAssetOps::FindSceneReferences(const std::string& relPath) {
    std::vector<std::string> scenes;
    if (relPath.empty())
        return scenes;

    const std::string needle = AssetPathFromRelative(relPath);
    const std::string needleAlt = NormalizeSlashes(relPath);
    const std::string selfAbs = GenericLower(PathFromUtf8(AbsolutePathFromRelative(relPath)));

    const std::vector<ProjectAssetIndex::ProjectEntry> sceneFiles =
        ProjectAssetIndex::Instance().GetFilesOfKind(ProjectAssetIndex::Kind::Scene);

    for (const ProjectAssetIndex::ProjectEntry& scene : sceneFiles) {
        const std::string sceneAbs = GenericLower(PathFromUtf8(AbsolutePathFromRelative(scene.relativePath)));
        if (!selfAbs.empty() && sceneAbs == selfAbs)
            continue;

        std::ifstream file(PathFromUtf8(scene.assetPath));
        if (!file)
            file.open(PathFromUtf8(AbsolutePathFromRelative(scene.relativePath)));
        if (!file)
            continue;

        std::ostringstream buffer;
        buffer << file.rdbuf();
        const std::string contents = buffer.str();
        const bool hit = contents.find(needle) != std::string::npos ||
            (!needleAlt.empty() && needleAlt != needle &&
                contents.find(needleAlt) != std::string::npos);
        if (hit)
            scenes.push_back(scene.relativePath);
    }
    return scenes;
}

bool ProjectAssetOps::RenameAsset(const std::string& oldRel, const std::string& newRel, std::string& error) {
    error.clear();
    if (oldRel.empty()) {
        error = "Cannot rename the project root.";
        return false;
    }

    const std::string newName = LeafName(newRel);
    if (!IsValidLeafName(newName, error))
        return false;

    if (ParentRel(NormalizeSlashes(oldRel)) != ParentRel(NormalizeSlashes(newRel))) {
        error = "Rename must stay in the same folder.";
        return false;
    }

    if (IsCurrentSceneOpen(oldRel) || FolderContainsCurrentScene(oldRel)) {
        error = "close scene first";
        return false;
    }

    const fs::path src(PathFromUtf8(AbsolutePathFromRelative(oldRel)));
    const fs::path dst(PathFromUtf8(AbsolutePathFromRelative(newRel)));
    std::error_code existsEc;
    if (!fs::exists(src, existsEc)) {
        error = "Asset not found.";
        return false;
    }
    if (fs::exists(dst, existsEc)) {
        error = "A file or folder with that name already exists.";
        return false;
    }

    std::error_code renameEc;
    fs::rename(src, dst, renameEc);
    if (renameEc) {
        error = "Rename failed: " + renameEc.message();
        return false;
    }
    return true;
}

bool ProjectAssetOps::DeleteAssetToRecycleBin(const std::string& relPath, std::string& error) {
    error.clear();
    if (relPath.empty()) {
        error = "Cannot delete the project root.";
        return false;
    }
    if (IsCurrentSceneOpen(relPath)) {
        error = "This scene is currently open";
        return false;
    }
    if (FolderContainsCurrentScene(relPath) && !IsCurrentSceneOpen(relPath)) {
        error = "This folder contains the currently open scene";
        return false;
    }

    const fs::path abs(PathFromUtf8(AbsolutePathFromRelative(relPath)));
    std::error_code existsEc;
    if (!fs::exists(abs, existsEc)) {
        error = "Asset not found.";
        return false;
    }

    std::string recycleError;
    if (RecyclePath(abs, recycleError))
        return true;

    std::error_code removeEc;
    if (fs::is_directory(abs, existsEc))
        fs::remove_all(abs, removeEc);
    else
        fs::remove(abs, removeEc);

    if (removeEc) {
        error = "Delete failed: " + removeEc.message();
        return false;
    }
    return true;
}

bool ProjectAssetOps::CreateFolder(const std::string& parentRel, const std::string& name, std::string& error) {
    error.clear();
    if (!IsValidLeafName(name, error))
        return false;

    const std::string childRel = parentRel.empty()
        ? name
        : NormalizeSlashes(parentRel) + "/" + name;
    const fs::path abs(PathFromUtf8(AbsolutePathFromRelative(childRel)));
    std::error_code existsEc;
    if (fs::exists(abs, existsEc)) {
        error = "A file or folder with that name already exists.";
        return false;
    }

    std::error_code createEc;
    if (!fs::create_directory(abs, createEc)) {
        error = createEc ? "Create folder failed: " + createEc.message()
            : "Create folder failed.";
        return false;
    }
    return true;
}

bool ProjectAssetOps::CreateMinimalScene(const std::string& parentRel, const std::string& name, std::string& error) {
    error.clear();
    std::string fileName = name;
    if (fileName.size() >= 5) {
        const std::string ext = ToLowerCopy(fileName.substr(fileName.size() - 5));
        if (ext != ".json")
            fileName += ".json";
    }
    else {
        fileName += ".json";
    }

    const std::string stem = fileName.substr(0, fileName.size() - 5);
    if (!IsValidLeafName(stem, error))
        return false;
    if (fileName.find_first_of("/\\") != std::string::npos) {
        error = "Name cannot contain path separators.";
        return false;
    }

    const std::string childRel = parentRel.empty()
        ? fileName
        : NormalizeSlashes(parentRel) + "/" + fileName;
    const fs::path abs(PathFromUtf8(AbsolutePathFromRelative(childRel)));
    std::error_code existsEc;
    if (fs::exists(abs, existsEc)) {
        error = "A file or folder with that name already exists.";
        return false;
    }

    std::ofstream file(abs);
    if (!file) {
        error = "Could not create scene file.";
        return false;
    }

    file <<
        "{\n"
        "    \"version\": 4,\n"
        "    \"settings\": {\n"
        "        \"render\": {\n"
        "            \"skyboxEnabled\": true,\n"
        "            \"skyboxCubemap\": \"rom/skybox/MySkybox.dds\",\n"
        "            \"skyboxTint\": { \"r\": 1.0, \"g\": 1.0, \"b\": 1.0, \"a\": 1.0 },\n"
        "            \"skyboxExposure\": 1.0,\n"
        "            \"skyboxRotationYDeg\": 0.0,\n"
        "            \"clearColor\": { \"r\": 0.12, \"g\": 0.14, \"b\": 0.18, \"a\": 1.0 },\n"
        "            \"defaultShader\": \"standard\",\n"
        "            \"shadowQuality\": \"medium\"\n"
        "        }\n"
        "    },\n"
        "    \"nodes\": [],\n"
        "    \"canvas\": {\n"
        "        \"name\": \"empty\",\n"
        "        \"elements\": []\n"
        "    }\n"
        "}\n";
    if (!file) {
        error = "Failed to write scene file.";
        return false;
    }
    return true;
}

bool ProjectAssetOps::ShowInExplorer(const std::string& relPath, bool selectItem, std::string& error) {
    error.clear();
    const fs::path abs(PathFromUtf8(AbsolutePathFromRelative(relPath)));
    std::error_code existsEc;
    if (!fs::exists(abs, existsEc)) {
        error = "Path not found.";
        return false;
    }

    std::wstring wide = abs.lexically_normal().wstring();
    for (wchar_t& c : wide) {
        if (c == L'/')
            c = L'\\';
    }

    HINSTANCE result;
    if (selectItem) {
        const std::wstring params = L"/select,\"" + wide + L"\"";
        result = ShellExecuteW(nullptr, L"open", L"explorer.exe", params.c_str(), nullptr, SW_SHOWNORMAL);
    }
    else {
        result = ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    const INT_PTR code = reinterpret_cast<INT_PTR>(result);
    if (code <= 32) {
        error = "Could not open Explorer.";
        return false;
    }
    return true;
}
