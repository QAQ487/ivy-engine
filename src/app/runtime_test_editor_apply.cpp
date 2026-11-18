// runtime_test_editor_apply.cpp
// Applies validated editor-bridge ops on the engine main thread.
// Must not steal focus or force the Editor window to the foreground.

#include "src/app/runtime_test_editor_apply.h"

#if defined(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE)
#include "engine/core/engine_gui.h"
#include "engine/scene/render_settings.h"
#if !defined(GAME_RUNTIME)
#include "engine/core/engine_gui_internal.h"
#endif
#include <string>

bool RuntimeTestEditor_Apply(const ivy::testing::EditorOpRequest& op, std::string& errorOut) {
    errorOut.clear();
    using ivy::testing::EditorOpKind;
    using ivy::testing::EditorPlayModeKind;

    switch (op.kind) {
    case EditorOpKind::PlayEnter: {
#if defined(GAME_RUNTIME)
        errorOut = "unsupported_host";
        return false;
#else
        if (EngineGUI_IsPlaying()) {
            errorOut = "already_playing";
            return false;
        }
        PlayMode mode = PlayMode::Viewport;
        if (op.playMode == EditorPlayModeKind::Fullscreen)
            mode = PlayMode::Fullscreen;
        // EngineGUI_StartPlay does not activate/foreground the HWND.
        EngineGUI_StartPlay(mode);
        if (!EngineGUI_IsPlaying()) {
            errorOut = "play_enter_failed";
            return false;
        }
        return true;
#endif
    }
    case EditorOpKind::PlayExit: {
#if defined(GAME_RUNTIME)
        errorOut = "unsupported_host";
        return false;
#else
        if (!EngineGUI_IsPlaying())
            return true;
        EngineGUI_StopPlay();
        if (EngineGUI_IsPlaying()) {
            errorOut = "play_exit_failed";
            return false;
        }
        return true;
#endif
    }
    case EditorOpKind::LoadScene: {
#if defined(GAME_RUNTIME)
        errorOut = "unsupported_host";
        return false;
#else
        if (op.scenePath.empty()) {
            errorOut = "invalid_scene";
            return false;
        }
        // Bypass unsaved popup: bridge loads are explicit test ops.
        if (!EngineGUI_LoadFromFileCore(op.scenePath.c_str())) {
            errorOut = "load_scene_failed";
            return false;
        }
        return true;
#endif
    }
    case EditorOpKind::SetSetting: {
        RenderSettingsData& rs = RenderSettings_Get();
        if (op.settingKey == "frustumCullingEnabled") {
            rs.frustumCullingEnabled = op.settingBoolValue;
            return true;
        }
        if (op.settingKey == "shadowCascadeCullingEnabled") {
            rs.shadowCascadeCullingEnabled = op.settingBoolValue;
            return true;
        }
        if (op.settingKey == "spriteCullingEnabled") {
            rs.spriteCullingEnabled = op.settingBoolValue;
            return true;
        }
        if (op.settingKey == "lodEnabled") {
            rs.lodEnabled = op.settingBoolValue;
            return true;
        }
        if (op.settingKey == "animCullingEnabled") {
            rs.animCullingEnabled = op.settingBoolValue;
            return true;
        }
        errorOut = "unknown_setting";
        return false;
    }
    }
    errorOut = "unknown_editor_op";
    return false;
}
#endif
