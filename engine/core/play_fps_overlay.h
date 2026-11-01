// play_fps_overlay.h
// In-play FPS overlay helpers.

#pragma once
#include "engine/core/content_path.h"
#include "libraries/nlohmann/json.hpp"
#include <atomic>
#include <fstream>

namespace PlayFpsOverlay {
inline constexpr const char* kPrefsPath = "rom/editor/ui_prefs.json";
inline std::atomic<bool> enabled{false};

inline bool IsEnabled() { return enabled.load(std::memory_order_relaxed); }
inline void SetEnabled(bool value) { enabled.store(value, std::memory_order_relaxed); }
inline bool ShouldDraw(bool playing) { return playing && IsEnabled(); }

inline void LoadPrefs(const nlohmann::json& prefs) {
    const auto found = prefs.is_object() ? prefs.find("showFpsWhenPlaying") : prefs.end();
    SetEnabled(found != prefs.end() && found->is_boolean() && found->get<bool>());
}

inline void WritePrefs(nlohmann::json& prefs) {
    prefs["showFpsWhenPlaying"] = IsEnabled();
}

inline void LoadFromFile(const char* path = kPrefsPath) {
    SetEnabled(false);
    try {
        std::ifstream file(content_path::ReadableFileCurrent(path));
        if (!file.is_open()) return;
        nlohmann::json prefs;
        file >> prefs;
        LoadPrefs(prefs);
    } catch (...) {
        SetEnabled(false);
    }
}
}
