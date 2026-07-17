#include "engine_gui.h"
#include "engine_gui_internal.h"
#include "engine/scene/scene_loader.h"
#include "engine/editor/project_manager.h"

#include <filesystem>

bool EngineGUI_SaveToFile(const char* filename) {
    return SceneLoader_SaveToFile(filename);
}

bool EngineGUI_LoadFromFileCore(const char* filename) {
    if (g_isPlaying)
        ReturnToStart();

    if (!SceneLoader_LoadFromFile(filename))
        return false;

    g_selNode = nullptr;
    g_selUiElement = nullptr;
    EngineGUI_SetEditMode(EditorEditMode::Scene3D);
    EngineGUI_ClearSceneDirty();

    if (filename && filename[0] && ProjectManager::Instance().IsOpen()) {
        const std::filesystem::path path(filename);
        ProjectManager::Instance().SetLastOpenedScene(path.filename().string());
    }
    return true;
}

bool EngineGUI_LoadFromFile(const char* filename) {
    if (g_isPlaying)
        ReturnToStart();

    if (EngineGUI_IsSceneDirty()) {
        EngineGUI_QueueUnsavedLoad(filename);
        return false;
    }

    return EngineGUI_LoadFromFileCore(filename);
}
