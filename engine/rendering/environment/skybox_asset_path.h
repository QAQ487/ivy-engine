// skybox_asset_path.h
// Shared helpers for Asset Browser drag-drop / "Set as > Skybox" path resolution.
// Header-only, no DirectX. Matches the Rendering Settings .png/.dds filter.

#pragma once

#include "engine/core/content_path.h"

#include <cctype>
#include <string>
#include <string_view>

namespace skybox_asset {

inline std::string LowerExtension(std::string_view path) {
    if (path.empty())
        return {};
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t base = (slash == std::string_view::npos) ? 0 : slash + 1;
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string_view::npos || dot < base)
        return {};
    std::string ext;
    ext.reserve(path.size() - dot);
    for (std::size_t i = dot; i < path.size(); ++i)
        ext.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(path[i]))));
    return ext;
}

// True for equirect PNG or legacy DDS cubemap paths the skybox field accepts.
inline bool IsEligibleCubemapPath(std::string_view path) {
    if (path.empty())
        return false;
    const std::string ext = LowerExtension(path);
    return ext == ".png" || ext == ".dds";
}

inline bool IsEligibleCubemapPath(const char* path) {
    if (!path)
        return false;
    return IsEligibleCubemapPath(std::string_view{path});
}

// Normalize an Asset Browser / drag-drop asset path for settings.render.skyboxCubemap.
// Returns empty when the path is not a safe rom/ .png|.dds (wrong ext, absolute,
// traversal, missing rom/ prefix, scripts tree, etc.).
inline std::string RomPathFromAssetPath(std::string_view path) {
    if (!IsEligibleCubemapPath(path))
        return {};
    std::string normalized;
    if (!content_path::IsSafeRomPath(path, &normalized))
        return {};
    return normalized;
}

inline std::string RomPathFromAssetPath(const char* path) {
    if (!path)
        return {};
    return RomPathFromAssetPath(std::string_view{path});
}

}  // namespace skybox_asset
