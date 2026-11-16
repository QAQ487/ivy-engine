// editor_menu_bar.cpp
// Extracted from engine/core/engine_gui.cpp (phase-1 editor UI split; behaviour-preserving).

#include "editor_gui_internal.h"

#include "engine/core/engine_gui.h"
#include "engine/core/play_fps_overlay.h"
#include "engine/core/job_system.h"
#include "engine/core/debug_system.h"
#include "engine/core/content_path.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/camera_component.h"
#include "engine/ecs/collider_component_access.h"
#include "engine/ecs/visual_component.h"
#include "engine/ecs/transform.h"
#include "engine/editor/editor_theme.h"
#include "engine/editor/editor_undo.h"
#include "engine/editor/editor_ui_panel.h"
#include "engine/editor/project_manager.h"
#include "engine/physics/collider/collider.h"
#include "engine/scene/scene_loader.h"
#include "engine/scripting/script_factory.h"
#include "debug_camera.h"
#include "imgui/imgui.h"
#include "imgui/icons_font_awesome.h"

#include <cstdio>
#include <memory>
#include <string>

static std::unique_ptr<GameObject> CreateModelGameObject(
    const std::string& modelName, const std::string& displayName)
{
    auto node = std::make_unique<GameObject>(displayName.empty() ? modelName : displayName);
    node->AddComponent<engine::Transform>();
    auto* vc = node->AddComponent<VisualComponent>();
    vc->modelPath = modelName;
    node->AddComponent<ColliderComponent>();
    return node;
}


namespace {
    const PrimitiveCreateSpec kDefaultPrimitives[] = {
        { "Cube",     "Cube",     "rom/model/primitives/Cube.fbx",     ColliderType::BOX,      1.0f },
        { "Sphere",   "Sphere",   "rom/model/primitives/Sphere.fbx",   ColliderType::SPHERE,   1.0f },
        { "Cylinder", "Cylinder", "rom/model/primitives/Cylinder.fbx", ColliderType::CYLINDER, 1.0f },
        { "Cone",     "Cone",     "rom/model/primitives/Cone.fbx",     ColliderType::MESH,     1.0f },
        // CapsuleCollider height = cylinder segment (sphere centers), not total
        // extent. Mesh is total H=2 with r=0.5 → segment = 2 - 2*r = 1.
        { "Capsule",  "Capsule",  "rom/model/primitives/Capsule.fbx",  ColliderType::CAPSULE,  1.0f },
        { "Donut",    "Donut",    "rom/model/primitives/Donut.fbx",    ColliderType::MESH,     1.0f },
    };
}

static std::unique_ptr<GameObject> CreatePrimitiveGameObject(const PrimitiveCreateSpec& spec)
{
    auto node = CreateModelGameObject(spec.assetPath, spec.displayName);
    if (auto* col = FindColliderComponent(node)) {
        col->colliderType = spec.colliderType;
        col->radius = 0.5f;
        col->halfExtents = { 0.5f, 0.5f, 0.5f };
        col->height = spec.height;
    }
    return node;
}

void DrawEditorMenuBar() {
    const bool canMutate = !g_isPlaying && !IsSceneLoadBusy();
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Project...", nullptr, false, canMutate))
            g_pendingFolderPick = PendingFolderPickAction::NewProject;
        if (ImGui::MenuItem("Open Project...", nullptr, false, canMutate))
            g_pendingFolderPick = PendingFolderPickAction::OpenProject;
        if (ImGui::MenuItem("Open Testbed", nullptr, false,
                            canMutate && !ProjectManager::Instance().ResolveInRepoTestbedPath().empty()))
            RequestOpenTestbed();
        if (ImGui::MenuItem("Close Project", nullptr, false,
                            canMutate && ProjectManager::Instance().IsOpen()))
            RequestCloseProject();
        if (ImGui::MenuItem("Save Project", "Ctrl+Shift+S", false, canMutate && ProjectManager::Instance().IsOpen()))
            SaveProject();

        if (ImGui::BeginMenu("Recent Projects")) {
            const auto& recent = ProjectManager::Instance().GetRecentProjects();
            if (recent.empty()) {
                ImGui::MenuItem("(empty)", nullptr, false, false);
            }
            else {
                for (size_t i = 0; i < recent.size(); ++i) {
                    const std::string& path = recent[i];
                    const bool isCurrent =
                        ProjectManager::Instance().IsOpen() &&
                        path == ProjectManager::Instance().GetRootPath();
                    char label[512];
                    snprintf(label, sizeof(label), "%zu  %s", i + 1, path.c_str());
                    if (ImGui::MenuItem(label, nullptr, isCurrent, canMutate && !isCurrent))
                        RequestOpenProject(path);
                }
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        {
            const bool canBuild = canMutate && ProjectManager::Instance().IsOpen() &&
                                  !PlayablePackage::IsBuildRunning();
            if (ImGui::MenuItem("Build Current Project...", nullptr, false, canBuild))
                RequestBuildCurrentProject();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, canMutate))
            SaveScene();
        if (ImGui::MenuItem("New Scene...", nullptr, false, canMutate))
            g_showNewScenePopup = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Clear Scene...", nullptr, false, canMutate))
            ImGui::OpenPopup("ClearSceneConfirm");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        const bool canEdit = !g_isPlaying && !EngineGUI_IsSceneAssetPreloadActive();
        auto& undoStack = EditorUndoStack::Instance();
        const std::string undoDesc = undoStack.TopUndoDescribe();
        const std::string redoDesc = undoStack.TopRedoDescribe();
        char undoLabel[256];
        char redoLabel[256];
        if (undoDesc.empty())
            snprintf(undoLabel, sizeof(undoLabel), "Undo");
        else
            snprintf(undoLabel, sizeof(undoLabel), "Undo %s", undoDesc.c_str());
        if (redoDesc.empty())
            snprintf(redoLabel, sizeof(redoLabel), "Redo");
        else
            snprintf(redoLabel, sizeof(redoLabel), "Redo %s", redoDesc.c_str());

        if (ImGui::MenuItem(undoLabel, "Ctrl+Z", false, canEdit && undoStack.CanUndo())) {
            if (EditorUndo_Undo()) {
                EngineGUI_MarkSceneDirty();
                SyncSceneRuntime();
            }
        }
        if (ImGui::MenuItem(redoLabel, "Ctrl+Y", false, canEdit && undoStack.CanRedo())) {
            if (EditorUndo_Redo()) {
                EngineGUI_MarkSceneDirty();
                SyncSceneRuntime();
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("GameObject")) {
        if (g_isPlaying)
            ImGui::BeginDisabled(true);
        if (ImGui::MenuItem("Create Empty"))
            SpawnRootGameObject(CreateEmptyGameObject());
        if (ImGui::BeginMenu("3D Object")) {
            for (const PrimitiveCreateSpec& spec : kDefaultPrimitives) {
                if (ImGui::MenuItem(spec.menuLabel))
                    SpawnRootGameObject(CreatePrimitiveGameObject(spec));
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Camera")) {
            if (ImGui::MenuItem("Camera")) SpawnRootGameObject(CreateCameraObject());
            if (ImGui::MenuItem("Debug Camera")) {
                SpawnRootGameObject(CreateCameraObject(true));
                if(g_selNode) {
                    EngineGUI_SetCameraMode(CameraMode::SceneCamera);
                    EngineGUI_SetActiveCameraNodeName(g_selNode->GetName());
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Light")) {
            if (ImGui::MenuItem("Directional Light"))
                SpawnRootGameObject(CreateDirectionalLightObject());
            if (ImGui::MenuItem("Point Light"))
                SpawnRootGameObject(CreatePointLightObject());
            if (ImGui::MenuItem("Spot Light"))
                SpawnRootGameObject(CreateSpotLightObject());
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("UI")) {
            if (ImGui::MenuItem("Button")) {
                g_selUiElement = EngineGUIUI::CreateUiElement(EngineGUIUI::UiCreateType::Button);
                g_selNode = nullptr;
                if (g_selUiElement) {
                    EditorUndo_PushUiCreate(g_selUiElement);
                    EngineGUI_MarkSceneDirty();
                }
            }
            if (ImGui::MenuItem("Text")) {
                g_selUiElement = EngineGUIUI::CreateUiElement(EngineGUIUI::UiCreateType::Text);
                g_selNode = nullptr;
                if (g_selUiElement) {
                    EditorUndo_PushUiCreate(g_selUiElement);
                    EngineGUI_MarkSceneDirty();
                }
            }
            if (ImGui::MenuItem("Sprite")) {
                g_selUiElement = EngineGUIUI::CreateUiElement(EngineGUIUI::UiCreateType::Sprite);
                g_selNode = nullptr;
                if (g_selUiElement) {
                    EditorUndo_PushUiCreate(g_selUiElement);
                    EngineGUI_MarkSceneDirty();
                }
            }
            if (ImGui::MenuItem("Bar")) {
                g_selUiElement = EngineGUIUI::CreateUiElement(EngineGUIUI::UiCreateType::Bar);
                g_selNode = nullptr;
                if (g_selUiElement) {
                    EditorUndo_PushUiCreate(g_selUiElement);
                    EngineGUI_MarkSceneDirty();
                }
            }
            ImGui::EndMenu();
        }
        if (g_isPlaying)
            ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window")) {
        if (ImGui::MenuItem("Rendering Settings", nullptr, g_showRenderingSettings))
            g_showRenderingSettings = !g_showRenderingSettings;
        if (ImGui::MenuItem("PostFX Showcase", nullptr, g_showPostFxShowcase))
            g_showPostFxShowcase = !g_showPostFxShowcase;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Project Settings")) {
        if (g_isPlaying)
            ImGui::BeginDisabled(true);
        if (ImGui::MenuItem("Default Scene..."))
            g_showDefaultScenePopup = true;
        if (ImGui::MenuItem("Gravity...", nullptr, false, ProjectManager::Instance().IsOpen()))
            g_showGravityPopup = true;
        if (ImGui::MenuItem("Audio...", nullptr, false, ProjectManager::Instance().IsOpen()))
            g_showAudioPopup = true;
        if (g_isPlaying)
            ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Engine Settings")) {
        if (ImGui::MenuItem("Show FPS When Playing", nullptr, PlayFpsOverlay::IsEnabled())) {
            PlayFpsOverlay::SetEnabled(!PlayFpsOverlay::IsEnabled());
            SaveUiPrefs();
        }
        if (ImGui::BeginMenu("UI Scale")) {
            for (int percent : kUiZoomPercents) {
                char label[16];
                snprintf(label, sizeof(label), "%d%%", percent);
                if (ImGui::MenuItem(label, nullptr, g_uiZoomPercent == percent)) {
                    ApplyUiZoom(percent);
                    SaveUiPrefs();
                    ProjectManager::Instance().SetEditorUiZoomPercent(percent);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("UI Style")) {
            for (int i = 0; i < static_cast<int>(editor_theme::StylePreset::Count); ++i) {
                const auto preset = static_cast<editor_theme::StylePreset>(i);
                const char* name = editor_theme::StylePresetName(preset);
                if (ImGui::MenuItem(name, nullptr, g_uiStylePreset == preset)) {
                    ApplyEditorTheme(preset, g_uiColorScheme);
                    ApplyUiZoom(g_uiZoomPercent);
                    SaveUiPrefs();
                    ProjectManager::Instance().SetEditorUiStylePreset(name);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Color Scheme")) {
            for (int i = 0; i < static_cast<int>(editor_theme::ColorScheme::Count); ++i) {
                const auto scheme = static_cast<editor_theme::ColorScheme>(i);
                const char* name = editor_theme::ColorSchemeName(scheme);
                if (ImGui::MenuItem(name, nullptr, g_uiColorScheme == scheme)) {
                    ApplyEditorTheme(g_uiStylePreset, scheme);
                    ApplyUiZoom(g_uiZoomPercent);
                    SaveUiPrefs();
                    ProjectManager::Instance().SetEditorUiColorScheme(name);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Shortcut Hints...", nullptr, g_showShortcutHints))
            g_showShortcutHints = !g_showShortcutHints;
        ImGui::EndMenu();
    }
}

