// content_path.h
// Safe relative paths for content projects. No path may escape its root.

#pragma once

#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>

namespace content_path {

// In-repo content project the Debug editor opens when no recent project loads.
inline constexpr const char* kInRepoTestbedDir = "testbed";

// Engine rom/ directories the testbed links instead of copying.
// "scene" is owned by the testbed. "editor" is writable UI prefs.
inline constexpr const char* kSharedRomDirNames[] = {
    "branding",
    "font",
    "model",
    "physmat",
    "shader",
    "skybox",
    "texture",
};
inline constexpr std::size_t kSharedRomDirCount =
    sizeof(kSharedRomDirNames) / sizeof(kSharedRomDirNames[0]);

// Relative path with no absolute prefix and no "." / ".." segment.
// Backslashes are normalized to '/'. Writes the normalized form when non-null.
inline bool IsSafeRelativePath(std::string_view path, std::string* normalized) {
    if (path.empty() || path.size() > 1024)
        return false;

    std::string n;
    n.reserve(path.size());
    for (unsigned char c : path) {
        if (c < 0x20)
            return false;
        n.push_back(c == '\\' ? '/' : static_cast<char>(c));
    }
    if (n[0] == '/')
        return false;
    if (n.size() >= 2 &&
        std::isalpha(static_cast<unsigned char>(n[0])) &&
        n[1] == ':') {
        return false;
    }

    std::size_t start = 0;
    while (start <= n.size()) {
        const std::size_t slash = n.find('/', start);
        const std::size_t count =
            (slash == std::string::npos) ? std::string::npos : slash - start;
        const std::string seg = n.substr(start, count);
        if (seg.empty() || seg == "." || seg == "..")
            return false;
        if (seg.front() == '.' || seg.back() == '.' || seg.back() == ' ')
            return false;
        if (seg.find_first_of("<>:\"|?*") != std::string::npos)
            return false;
        if (slash == std::string::npos)
            break;
        start = slash + 1;
        if (start == n.size())
            return false;
    }

    if (normalized)
        *normalized = std::move(n);
    return true;
}

// Safe relative path whose first segment is exactly "rom" and that names a file
// under that directory (at least rom/<file>).
inline bool IsSafeRomPath(std::string_view path, std::string* normalized) {
    std::string n;
    if (!IsSafeRelativePath(path, &n))
        return false;
    if (n.size() < 5 || n.compare(0, 4, "rom/") != 0)
        return false;
    if (normalized)
        *normalized = std::move(n);
    return true;
}

// Child of `root` using only lexical normalization. Symlinks and NTFS junctions
// are not resolved, so a link at rom/model stays "model" instead of "../../rom/model".
// Returns empty when the path is not a strict descendant (`..`, different roots,
// or `root` itself).
inline std::filesystem::path LexicalChildPath(const std::filesystem::path& path,
                                              const std::filesystem::path& root) {
    auto asChild = [](const std::filesystem::path& candidate,
                      const std::filesystem::path& base) -> std::filesystem::path {
        const std::filesystem::path rel =
            candidate.lexically_normal().lexically_relative(base.lexically_normal());
        if (rel.empty() || rel == "." || rel.is_absolute())
            return {};
        for (const std::filesystem::path& part : rel) {
            if (part == "..")
                return {};
        }
        return rel;
    };

    const std::filesystem::path direct = asChild(path, root);
    if (!direct.empty())
        return direct;

    std::error_code ec;
    const std::filesystem::path absPath = std::filesystem::absolute(path, ec);
    if (ec)
        return {};
    ec.clear();
    const std::filesystem::path absRoot = std::filesystem::absolute(root, ec);
    if (ec)
        return {};
    // absolute() does not resolve symlinks or junctions.
    return asChild(absPath, absRoot);
}

// True when `candidate` canonicalizes to a strict descendant of `root`.
// `..`, a missing path, or `root` itself returns false.
inline bool IsInsideDirectory(const std::filesystem::path& root,
                              const std::filesystem::path& candidate) {
    std::error_code ec;
    if (!std::filesystem::exists(candidate, ec))
        return false;
    ec.clear();
    const std::filesystem::path rootCanon = std::filesystem::weakly_canonical(root, ec);
    if (ec)
        return false;
    ec.clear();
    const std::filesystem::path candCanon =
        std::filesystem::weakly_canonical(candidate, ec);
    if (ec)
        return false;
    const std::filesystem::path rel = candCanon.lexically_relative(rootCanon);
    if (rel.empty() || rel == ".")
        return false;
    for (const std::filesystem::path& part : rel) {
        if (part == "..")
            return false;
    }
    return true;
}

// Walk up from a cwd or an exe directory. The engine repo is the folder that
// contains both external_content_template/ and IvyEngine.sln (or Game_Project.sln).
inline std::filesystem::path FindEngineRepoRootFromSeed(const std::filesystem::path& seed) {
    if (seed.empty())
        return {};
    std::error_code ec;
    std::filesystem::path cur = seed;
    for (int i = 0; i < 8; ++i) {
        const bool marker = std::filesystem::is_directory(cur / "external_content_template", ec);
        ec.clear();
        const bool solution =
            std::filesystem::is_regular_file(cur / "IvyEngine.sln", ec) ||
            std::filesystem::is_regular_file(cur / "Game_Project.sln", ec);
        ec.clear();
        if (marker && solution)
            return cur;
        if (!cur.has_parent_path() || cur == cur.parent_path())
            break;
        cur = cur.parent_path();
    }
    return {};
}

// Canonical testbed path under the engine repo, or empty when it is missing
// or would leave that repo. The folder name is the fixed constant, not user input.
inline std::filesystem::path ResolveInRepoTestbedPath(const std::filesystem::path& cwd,
                                                      const std::filesystem::path& exeDir) {
    if (!IsSafeRelativePath(kInRepoTestbedDir, nullptr))
        return {};
    std::filesystem::path repo = FindEngineRepoRootFromSeed(cwd);
    if (repo.empty())
        repo = FindEngineRepoRootFromSeed(exeDir);
    if (repo.empty())
        return {};

    std::error_code ec;
    const std::filesystem::path repoCanon = std::filesystem::weakly_canonical(repo, ec);
    if (ec)
        return {};
    const std::filesystem::path testbed = repoCanon / kInRepoTestbedDir;
    if (!IsInsideDirectory(repoCanon, testbed))
        return {};
    ec.clear();
    if (!std::filesystem::is_directory(testbed / "rom", ec))
        return {};
    ec.clear();
    if (!std::filesystem::is_regular_file(testbed / "project.json", ec))
        return {};
    ec.clear();
    const std::filesystem::path canon = std::filesystem::weakly_canonical(testbed, ec);
    if (ec || !IsInsideDirectory(repoCanon, canon))
        return {};
    return canon;
}

// Open Project accepts any existing directory that contains rom/.
// That includes the in-repo testbed. The engine root and the exe output
// folder are not rejected by this rule.
inline bool ContentProjectHasRom(const std::filesystem::path& root) {
    std::error_code ec;
    return std::filesystem::is_directory(root, ec) &&
           std::filesystem::is_directory(root / "rom", ec);
}

}  // namespace content_path
