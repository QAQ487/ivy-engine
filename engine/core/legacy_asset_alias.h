// legacy_asset_alias.h
// Old content names the bitmap font atlas the engine used to ship
// (rom/texture/consolab_ascii_512.png). The engine no longer ships it: UI text uses
// TrueType fonts. A content file that still names the atlas is read as the default
// UI font instead, unless the project provides its own atlas file at that path.
// Pure logic (no Windows / D3D), unit-testable on Linux.

#pragma once

#include <string>

namespace legacy_alias {

inline constexpr const char* kLegacyFontAtlas = "rom/texture/consolab_ascii_512.png";
inline constexpr const char* kReplacementFont = "rom/font/LemonSlice.ttf";

// Case-insensitive, accepts '\\'. Only the exact legacy atlas path matches.
inline bool IsLegacyFontAtlas(const std::string& path) {
    const std::string legacy = kLegacyFontAtlas;
    if (path.size() != legacy.size())
        return false;
    for (std::size_t i = 0; i < path.size(); ++i) {
        char c = path[i];
        if (c == '\\')
            c = '/';
        else if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
        if (c != legacy[i])
            return false;
    }
    return true;
}

// `fileExists`: the project (or resources/) really has a file at `path`.
// Returns the path to use for the font.
inline std::string FontPath(const std::string& path, bool fileExists) {
    if (!fileExists && IsLegacyFontAtlas(path))
        return kReplacementFont;
    return path;
}

}  // namespace legacy_alias
