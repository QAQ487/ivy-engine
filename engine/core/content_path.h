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

}  // namespace content_path
