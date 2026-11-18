// builtin_paths.h
// Paths of engine-owned assets compiled into the exe. They use the "builtin:" prefix so they
// cannot be mistaken for project files: "rom/..." always means a file in the content
// project (or the engine's resources/ fallback), "builtin:..." never touches the disk and
// cannot be overridden. Light header (no asset data); the data lives in builtin_assets.h.

#pragma once

#include <string>

namespace builtin_paths {

inline constexpr const char* kUiFont = "builtin:font/IBMPlexMono-Regular.ttf";
inline constexpr const char* kBrandLogo = "builtin:branding/ivy_logo.png";
inline constexpr const wchar_t* kBrandLogoW = L"builtin:branding/ivy_logo.png";
inline constexpr const char* kBrdfLut = "builtin:ibl/brdf_lut.png";

// True for any "builtin:" path (case-insensitive prefix).
template <class Char>
inline bool IsBuiltin(const std::basic_string<Char>& path) {
    static const char kPrefix[] = "builtin:";
    if (path.size() < sizeof(kPrefix) - 1)
        return false;
    for (std::size_t i = 0; i < sizeof(kPrefix) - 1; ++i) {
        unsigned long c = static_cast<unsigned long>(path[i]);
        if (c >= 'A' && c <= 'Z')
            c = c - 'A' + 'a';
        if (c != static_cast<unsigned long>(kPrefix[i]))
            return false;
    }
    return true;
}

}  // namespace builtin_paths
