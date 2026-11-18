// legacy_asset_alias.h
// Old content names two fonts the engine no longer ships: the bitmap font atlas
// (rom/texture/consolab_ascii_512.png) and rom/font/LemonSlice.ttf. UI text now defaults to
// the built-in IBM Plex Mono ("builtin:font/IBMPlexMono-Regular.ttf", compiled into the exe).
// A content file that still names a retired font is read as the default UI font, unless the
// project provides its own file at that path.
// Pure logic (no Windows / D3D), unit-testable on Linux.

#pragma once

#include <string>

namespace legacy_alias {

inline constexpr const char* kLegacyFontAtlas = "rom/texture/consolab_ascii_512.png";
inline constexpr const char* kLegacyLemonSlice = "rom/font/LemonSlice.ttf";
inline constexpr const char* kReplacementFont = "builtin:font/IBMPlexMono-Regular.ttf";

namespace detail {

// Case-insensitive, accepts '\\'.
inline bool SamePath(const std::string& path, const std::string& expected) {
    if (path.size() != expected.size())
        return false;
    for (std::size_t i = 0; i < path.size(); ++i) {
        char c = path[i];
        if (c == '\\')
            c = '/';
        else if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
        char e = expected[i];
        if (e >= 'A' && e <= 'Z')
            e = static_cast<char>(e - 'A' + 'a');
        if (c != e)
            return false;
    }
    return true;
}

}  // namespace detail

// True for a retired font path (bitmap atlas or LemonSlice).
inline bool IsLegacyFontPath(const std::string& path) {
    return detail::SamePath(path, kLegacyFontAtlas) || detail::SamePath(path, kLegacyLemonSlice);
}

// `fileExists`: the project (or resources/) really has a file at `path`.
// Returns the path to use for the font.
inline std::string FontPath(const std::string& path, bool fileExists) {
    if (!fileExists && IsLegacyFontPath(path))
        return kReplacementFont;
    return path;
}

}  // namespace legacy_alias
