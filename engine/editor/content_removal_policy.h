// content_removal_policy.h
// Pure helpers for editor content-monitor removal decisions.
// Header-only, no Windows / D3D. Used by engine_gui and Linux unit tests.

#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace content_reload {

inline std::string LowerForwardSlashes(std::string_view path) {
    std::string out;
    out.reserve(path.size());
    for (unsigned char c : path) {
        if (c == '\\')
            out.push_back('/');
        else
            out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

// Lookup key for scene-JSON / cache comparisons: lowercase, '/', with rom/ prefix.
inline std::string NormalizeRomLookupKey(std::string_view path) {
    std::string key = LowerForwardSlashes(path);
    if (key.empty())
        return {};
    if (key == "rom" || key.compare(0, 4, "rom/") == 0)
        return key;
    return "rom/" + key;
}

// Same skip rule as "added file not in scene JSON", with slash/case/rom/ normalization.
inline bool SceneJsonReferencesRomPath(std::string_view sceneJson, std::string_view romPath) {
    const std::string key = NormalizeRomLookupKey(romPath);
    if (key.size() < 5)
        return false;
    const std::string hay = LowerForwardSlashes(sceneJson);
    return hay.find(key) != std::string::npos;
}

enum class RemovedContentAction {
    KeepWatching,         // not in scene JSON and not currently loaded
    ReportFailureRetain,  // referenced and/or loaded — keep resources, red light
};

inline RemovedContentAction DecideRemovedContentAction(
    std::string_view romPath,
    std::string_view sceneJson,
    bool currentlyLoaded)
{
    if (currentlyLoaded || SceneJsonReferencesRomPath(sceneJson, romPath))
        return RemovedContentAction::ReportFailureRetain;
    return RemovedContentAction::KeepWatching;
}

}  // namespace content_reload
