// editor_menu_bar.cpp
// Extracted from engine/core/engine_gui.cpp (phase-1 editor UI split; behaviour-preserving).

#include "editor_gui_common_includes.h"

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
    const bool playVisuals = g_isPlaying || g_holdPlayVisuals;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scene...", nullptr, false, canMutate))
            g_showNewScenePopup = true;
        if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, canMutate))
            SaveScene();
        if (ImGui::MenuItem("Clear Scene...", nullptr, false, canMutate))
            ImGui::OpenPopup("ClearSceneConfirm");

        ImGui::Separator();
        if (ImGui::MenuItem("New Project...", nullptr, false, canMutate))
            g_pendingFolderPick = PendingFolderPickAction::NewProject;
        if (ImGui::MenuItem("Open Project...", nullptr, false, canMutate))
            g_pendingFolderPick = PendingFolderPickAction::OpenProject;
        if (ImGui::MenuItem("Open Testbed", nullptr, false,
                            canMutate && !ProjectManager::Instance().ResolveInRepoTestbedPath().empty()))
            RequestOpenTestbed();
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
        if (ImGui::MenuItem("Close Project", nullptr, false,
                            canMutate && ProjectManager::Instance().IsOpen()))
            RequestCloseProject();
        if (ImGui::MenuItem("Save Project", "Ctrl+Shift+S", false, canMutate && ProjectManager::Instance().IsOpen()))
            SaveProject();

        ImGui::Separator();
        {
            const bool canBuild = canMutate && ProjectManager::Instance().IsOpen() &&
                                  !PlayablePackage::IsBuildRunning();
            if (ImGui::MenuItem("Build Current Project...", nullptr, false, canBuild))
                RequestBuildCurrentProject();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {
            if (HWND hwnd = GetWindowHandle())
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }
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
        ImGui::Separator();
        if (ImGui::MenuItem("Project Settings..."))
            EditorSettings_OpenProjectSettings();
        if (ImGui::MenuItem("Preferences..."))
            EditorSettings_OpenPreferences();
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

    if (ImGui::BeginMenu("View")) {
        if (playVisuals)
            ImGui::BeginDisabled();
        if (ImGui::MenuItem("Show Grid", nullptr, Grid_IsVisible()))
            Grid_SetVisible(!Grid_IsVisible());
        if (ImGui::MenuItem("Show Gizmos", nullptr, TransformGizmo::IsVisible()))
            TransformGizmo::SetVisible(!TransformGizmo::IsVisible());
        if (ImGui::MenuItem("Show UI Overlay", nullptr, g_showUiOverlay))
            g_showUiOverlay = !g_showUiOverlay;
        {
            bool view2D = DebugCamera_Is2DView();
            if (ImGui::MenuItem("2D View", nullptr, view2D))
                DebugCamera_Set2DView(!view2D);
        }
        if (playVisuals)
            ImGui::EndDisabled();

        ImGui::Separator();
        if (ImGui::MenuItem("Show FPS When Playing", nullptr, PlayFpsOverlay::IsEnabled())) {
            PlayFpsOverlay::SetEnabled(!PlayFpsOverlay::IsEnabled());
            SaveUiPrefs();
        }
        if (ImGui::MenuItem("Collision Visualizer", "F11", DebugSystem::CollisionVizEnabled()))
            DebugSystem::ToggleCollisionViz();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window")) {
        auto& panels = EditorDocking_PanelVisibility();
        ImGui::MenuItem(kEditorHierarchyWindowTitle, nullptr, &panels.hierarchy);
        ImGui::MenuItem(kEditorPropertiesWindowTitle, nullptr, &panels.properties);
        ImGui::MenuItem(kEditorAssetBrowserWindowTitle, nullptr, &panels.assetBrowser);
        ImGui::Separator();
        ImGui::MenuItem("Project Settings", nullptr, &EditorSettings_ProjectWindowOpen());
        ImGui::MenuItem("Preferences", nullptr, &EditorSettings_PreferencesWindowOpen());
        if (ImGui::MenuItem("PostFX Showcase", nullptr, g_showPostFxShowcase))
            g_showPostFxShowcase = !g_showPostFxShowcase;
        if (ImGui::MenuItem("Shortcut Hints...", nullptr, g_showShortcutHints))
            g_showShortcutHints = !g_showShortcutHints;
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout"))
            EditorDocking_RequestResetLayout();
        ImGui::EndMenu();
    }
}

