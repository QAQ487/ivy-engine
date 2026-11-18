// editor_popups.cpp
// Extracted from engine/core/engine_gui.cpp (phase-1 editor UI split; behaviour-preserving).

#include "editor_gui_common_includes.h"
namespace fs = std::filesystem;

static void ExecutePendingEditorAction() {
    const PendingEditorAction action = g_pendingEditorAction;
    const std::string payload = g_pendingEditorSceneFile;
    const std::string projectPath = g_pendingProjectPath;
    const std::string createParent = g_pendingCreateProjectParent;
    const std::string createName = g_pendingCreateProjectName;

    g_pendingEditorAction = PendingEditorAction::None;
    g_pendingEditorSceneFile.clear();
    g_pendingProjectPath.clear();
    g_pendingCreateProjectParent.clear();
    g_pendingCreateProjectName.clear();
    g_showUnsavedChangesPopup = false;
    ImGui::CloseCurrentPopup();

    switch (action) {
    case PendingEditorAction::LoadScene:
        if (!payload.empty())
            EngineGUI_LoadFromFileCore(payload.c_str());
        break;

    case PendingEditorAction::NewScene:
        if (!payload.empty()) {
            SceneLoader_ClearSceneNodes();
            g_selNode = nullptr;
            g_selUiElement = nullptr;
            SceneLoader_CopySceneFileName(payload);
            SaveScene();
        }
        break;

    case PendingEditorAction::OpenProject:
        if (!projectPath.empty()) {
            std::string error;
            if (!ProjectManager::Instance().OpenProject(projectPath, &error)) {
                MessageBoxA(GetWindowHandle(),
                            error.empty() ? "Failed to open project." : error.c_str(),
                            "Open Project", MB_OK | MB_ICONERROR);
            }
            else {
                SwitchToOpenedProject();
            }
        }
        break;

    case PendingEditorAction::CreateProject:
        if (!createParent.empty()) {
            std::string error;
            if (!ProjectManager::Instance().CreateProject(createParent, createName, &error,g_newProject2D)) {
                MessageBoxA(GetWindowHandle(),
                            error.empty() ? "Failed to create project." : error.c_str(),
                            "New Project", MB_OK | MB_ICONERROR);
            }
            else {
                SwitchToOpenedProject();
            }
        }
        break;

    case PendingEditorAction::CloseProject:
        CloseOpenedProject();
        break;

    case PendingEditorAction::Exit:
        PostQuitMessage(0);
        break;

    case PendingEditorAction::None:
    default:
        break;
    }
}

void DrawProjectGravitySettings() {
    ImGui::Text("Project Gravity");
    ImGui::TextDisabled("Stored in project.json (physics.gravity)");
    ImGui::Separator();

    float gravity = ProjectManager::Instance().GetPhysicsGravity();
    ImGui::SetNextItemWidth(Ui(160.0f));
    if (ImGui::DragFloat("m/s^2", &gravity, 0.05f, 0.1f, 100.0f, "%.2f"))
        ProjectManager::Instance().SetPhysicsGravity(gravity);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        ProjectManager::Instance().SetPhysicsGravity(gravity);
        ProjectManager::Instance().SaveProjectFile(nullptr);
    }
}

void DrawProjectAudioSettings() {
    ImGui::Text("Project Audio");
    ImGui::TextDisabled("Stored in project.json (audio.*)");
    ImGui::Separator();

    auto& audio = AudioManager::Instance();
    float master = audio.GetMasterVolume();
    float bgm = audio.GetBGMVolume();
    float sfx = audio.GetSFXVolume();
    float voice = audio.GetVoiceVolume();
    auto saveBuses = [&]() {
        ProjectManager::Instance().SetAudioBuses(
            audio.GetMasterVolume(), audio.GetBGMVolume(),
            audio.GetSFXVolume(), audio.GetVoiceVolume());
        ProjectManager::Instance().SaveProjectFile(nullptr);
    };
    ImGui::SetNextItemWidth(Ui(200.0f));
    if (ImGui::SliderFloat("Master", &master, 0.0f, 1.0f, "%.2f")) {
        audio.SetMasterVolume(master);
        ProjectManager::Instance().SetAudioBuses(master, bgm, sfx, voice);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) saveBuses();
    ImGui::SetNextItemWidth(Ui(200.0f));
    if (ImGui::SliderFloat("BGM", &bgm, 0.0f, 1.0f, "%.2f")) {
        audio.SetBGMVolume(bgm);
        ProjectManager::Instance().SetAudioBuses(master, bgm, sfx, voice);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) saveBuses();
    ImGui::SetNextItemWidth(Ui(200.0f));
    if (ImGui::SliderFloat("SFX", &sfx, 0.0f, 1.0f, "%.2f")) {
        audio.SetSFXVolume(sfx);
        ProjectManager::Instance().SetAudioBuses(master, bgm, sfx, voice);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) saveBuses();
    ImGui::SetNextItemWidth(Ui(200.0f));
    if (ImGui::SliderFloat("Voice", &voice, 0.0f, 1.0f, "%.2f")) {
        audio.SetVoiceVolume(voice);
        ProjectManager::Instance().SetAudioBuses(master, bgm, sfx, voice);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) saveBuses();

    ImGui::Separator();
    ImGui::Text("UI Click Sound");
    ImGui::TextDisabled("WAV played when a UI button activates");
    char clickBuf[256];
    strncpy_s(clickBuf, ProjectManager::Instance().GetAudioSettings().uiClickSound.c_str(),
        sizeof(clickBuf) - 1);
    clickBuf[sizeof(clickBuf) - 1] = '\0';
    ImGui::SetNextItemWidth(Ui(280.0f));
    if (ImGui::InputText("##UiClickSound", clickBuf, sizeof(clickBuf)))
        ProjectManager::Instance().SetUiClickSound(clickBuf);
    if (ImGui::IsItemDeactivatedAfterEdit())
        ProjectManager::Instance().SaveProjectFile(nullptr);
    {
        const char* wavExts[] = { ".wav" };
        std::string droppedPath;
        if (AcceptAssetPathDrop(wavExts, IM_ARRAYSIZE(wavExts), droppedPath)) {
            ProjectManager::Instance().SetUiClickSound(droppedPath);
            ProjectManager::Instance().SaveProjectFile(nullptr);
        }
    }
}

void DrawProjectDefaultSceneSettings() {
    if (!SceneLoader_IsSceneAssetsScanned())
        SceneLoader_RefreshSceneAssets();

    ImGui::Text("Default Scene");
    ImGui::TextDisabled("Stored in project.json (defaultScene)");
    ImGui::TextDisabled("Current: %s", SceneLoader_GetDefaultSceneDisplayName().c_str());
    ImGui::Separator();

    const int assetCount = SceneLoader_GetSceneAssetCount();
    if (assetCount == 0) {
        ImGui::TextDisabled("No scenes found");
    }
    else {
        ImGui::BeginChild("##DefaultSceneList", ImVec2(0.0f, 240.0f), ImGuiChildFlags_Borders);
        for (int i = 0; i < assetCount; ++i) {
            std::string displayName, fileName;
            SceneLoader_TryGetSceneAsset(i, nullptr, &displayName, &fileName);
            const bool selected = (fileName == SceneLoader_GetDefaultSceneFileName());
            const std::string label = std::string(ICON_FA_MAP) + " " + displayName;
            if (ImGui::Selectable(label.c_str(), selected)) {
                SceneLoader_SetDefaultSceneFileName(fileName, true);
                ProjectManager::Instance().SetDefaultScene(fileName);
                // Persist immediately (same pattern as the old Gravity popup); otherwise
                // project.json keeps the old defaultScene and wins on next open.
                if (ProjectManager::Instance().IsOpen())
                    ProjectManager::Instance().SaveProjectFile(nullptr);
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndChild();
    }
}

bool DrawPlayablePackageConfigFields() {
    bool edited = false;
    ImGui::SetNextItemWidth(Ui(420.0f));
    ImGui::InputText("Output folder", g_buildDest, IM_ARRAYSIZE(g_buildDest));
    if (ImGui::IsItemDeactivatedAfterEdit())
        edited = true;
    ImGui::SameLine();
    if (ImGui::Button("Browse..."))
        g_pendingFolderPick = PendingFolderPickAction::BuildOutput;

    const char* configs[] = { "Debug", "Release" };
    ImGui::SetNextItemWidth(Ui(160.0f));
    if (ImGui::Combo("Configuration", &g_buildConfigIndex, configs, IM_ARRAYSIZE(configs)))
        edited = true;
    ImGui::TextDisabled("Play FPS overlay follows View > Show FPS When Playing in both configurations.");

    const std::string destReason = PlayablePackage::DestinationRefusalReason(
        ProjectManager::Instance().GetRootPath(), g_buildDest);
    if (!destReason.empty())
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "%s", destReason.c_str());
    return edited;
}

void DrawEditorPopups() {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("ClearSceneConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Clear all objects from the scene?");
        ImGui::TextDisabled("This cannot be undone.");
        ImGui::Separator();
        if (ImGui::Button("Clear", ImVec2(Ui(120.0f), 0))) {
            ClearScene();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(Ui(120.0f), 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (g_showNewScenePopup)
        ImGui::OpenPopup("NewScenePopup");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("NewScenePopup", &g_showNewScenePopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("New Scene Name");
        ImGui::SetNextItemWidth(Ui(260.0f));
        ImGui::InputText("##NewSceneNameModal", g_newSceneName, IM_ARRAYSIZE(g_newSceneName));
        ImGui::Separator();
        if (ImGui::Button("Create", ImVec2(Ui(120.0f), 0))) {
            if (EngineGUI_IsSceneDirty()) {
                const std::string sceneFile = NormalizeSceneFileName(g_newSceneName);
                if (!sceneFile.empty()) {
                    g_pendingEditorAction = PendingEditorAction::NewScene;
                    g_pendingEditorSceneFile = sceneFile;
                    g_showUnsavedChangesPopup = true;
                }
            }
            else {
                NewScene();
            }
            g_showNewScenePopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(Ui(120.0f), 0))) {
            g_showNewScenePopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (g_showNewProjectPopup)
        ImGui::OpenPopup("NewProjectPopup");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("NewProjectPopup", &g_showNewProjectPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Checkbox("2D Project",&g_newProject2D);
        ImGui::Text("New Project Name");
        ImGui::SetNextItemWidth(Ui(280.0f));
        ImGui::InputText("##NewProjectNameModal", g_newProjectName, IM_ARRAYSIZE(g_newProjectName));
        ImGui::TextDisabled("%s", g_newProjectParentDir.c_str());
        ImGui::Separator();
        if (ImGui::Button("Create", ImVec2(Ui(120.0f), 0))) {
            const std::string name = TrimString(g_newProjectName);
            if (EngineGUI_IsSceneDirty()) {
                g_pendingEditorAction = PendingEditorAction::CreateProject;
                g_pendingCreateProjectParent = g_newProjectParentDir;
                g_pendingCreateProjectName = name;
                g_showUnsavedChangesPopup = true;
            }
            else {
                std::string error;
                if (!ProjectManager::Instance().CreateProject(g_newProjectParentDir, name, &error,g_newProject2D)) {
                    MessageBoxA(GetWindowHandle(),
                                error.empty() ? "Failed to create project." : error.c_str(),
                                "New Project", MB_OK | MB_ICONERROR);
                }
                else {
                    SwitchToOpenedProject();
                }
            }
            g_showNewProjectPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(Ui(120.0f), 0))) {
            g_showNewProjectPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (g_showAddTagPopup)
        ImGui::OpenPopup("AddTagPopup");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("AddTagPopup", &g_showAddTagPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        static bool s_addTagFailed = false;
        ImGui::Text("New Tag Name");
        ImGui::SetNextItemWidth(Ui(260.0f));
        const bool enterPressed = ImGui::InputText("##NewTagNameModal", g_newTagName,
            IM_ARRAYSIZE(g_newTagName), ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::IsItemEdited())
            s_addTagFailed = false;
        if (s_addTagFailed)
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Name is empty or already exists.");
        ImGui::Separator();

        auto tryAddTag = [&]() {
            TagRegistry& tags = TagRegistry::Instance();
            const int newId = tags.AddTag(g_newTagName);
            if (newId < 0) {
                s_addTagFailed = true;
                return false;
            }
            tags.SaveToFile();
            if (g_selNode) {
                const int oldTag = g_selNode->GetTagId();
                g_selNode->SetTag(newId);
                EditorUndo_PushTag(g_selNode, oldTag, newId);
                EngineGUI_MarkSceneDirty();
            }
            s_addTagFailed = false;
            g_showAddTagPopup = false;
            ImGui::CloseCurrentPopup();
            return true;
        };

        if (ImGui::Button("Create", ImVec2(Ui(120.0f), 0)) || enterPressed)
            tryAddTag();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(Ui(120.0f), 0))) {
            s_addTagFailed = false;
            g_showAddTagPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (g_showDeleteTagPopup)
        ImGui::OpenPopup("DeleteTagPopup");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("DeleteTagPopup", &g_showDeleteTagPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        TagRegistry& tags = TagRegistry::Instance();
        const char* tagName = tags.IsValid(g_pendingDeleteTagId)
            ? tags.GetName(g_pendingDeleteTagId)
            : "(invalid)";
        ImGui::Text("Delete tag \"%s\"?", tagName);
        ImGui::TextWrapped(
            "Nodes using this tag will be set to NONE. "
            "Higher custom tag ids will be remapped.");
        ImGui::Separator();

        if (ImGui::Button("Delete", ImVec2(Ui(120.0f), 0))) {
            if (tags.CanRemove(g_pendingDeleteTagId)) {
                const int deletedId = g_pendingDeleteTagId;
                RemapSceneTagsAfterDelete(deletedId);
                tags.RemoveTag(deletedId);
                tags.SaveToFile();
                EngineGUI_MarkSceneDirty();
            }
            g_pendingDeleteTagId = -1;
            g_showDeleteTagPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(Ui(120.0f), 0))) {
            g_pendingDeleteTagId = -1;
            g_showDeleteTagPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (g_sceneReloadConflict && EngineGUI_IsSceneDirty() && !g_isPlaying && !IsSceneLoadBusy()
        && !ImGui::GetTopMostPopupModal() && !g_showUnsavedChangesPopup)
        ImGui::OpenPopup("SceneJsonChangedPopup");
    if (ImGui::BeginPopupModal("SceneJsonChangedPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!g_sceneReloadConflict || g_sceneReloadConflict->path != SceneLoader_GetCurrentScenePath()) {
            g_sceneReloadConflict.reset();
            ImGui::CloseCurrentPopup();
        } else {
            ImGui::TextUnformatted("The scene JSON changed outside the Editor.");
            ImGui::TextUnformatted("The Editor has unsaved changes.");
            if (ImGui::Button("Load disk version (discard Editor changes)")) {
                ResolveSceneReloadConflict(true);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Keep Editor changes")) {
                ResolveSceneReloadConflict(false);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

    if (g_showUnsavedChangesPopup)
        ImGui::OpenPopup("UnsavedChangesPopup");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("UnsavedChangesPopup", &g_showUnsavedChangesPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You have unsaved changes.");
        ImGui::TextDisabled("Save before continuing?");
        ImGui::Separator();

        if (ImGui::Button("Save", ImVec2(Ui(120.0f), 0))) {
            std::string leavingSceneFile;
            {
                const std::string& currentScenePath = SceneLoader_GetCurrentScenePath();
                if (!currentScenePath.empty()) {
                    leavingSceneFile = NormalizeSceneFileName(
                        PathToUtf8(PathFromUtf8(currentScenePath).filename()));
                }
            }
            if (leavingSceneFile.empty())
                leavingSceneFile = NormalizeSceneFileName(SceneLoader_GetSaveFileName());

            if (!leavingSceneFile.empty())
                SaveSceneToFile(leavingSceneFile);
            ExecutePendingEditorAction();
        }
        ImGui::SameLine();

        if (ImGui::Button("Don't Save", ImVec2(Ui(120.0f), 0))) {
            EngineGUI_ClearSceneDirty();
            ExecutePendingEditorAction();
        }
        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(Ui(120.0f), 0))) {
            g_pendingEditorAction = PendingEditorAction::None;
            g_pendingEditorSceneFile.clear();
            g_pendingProjectPath.clear();
            g_pendingCreateProjectParent.clear();
            g_pendingCreateProjectName.clear();
            g_showUnsavedChangesPopup = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (g_showBuildUnsavedPopup)
        ImGui::OpenPopup("BuildUnsavedPopup");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("BuildUnsavedPopup", &g_showBuildUnsavedPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Build %s?", ProjectManager::Instance().GetName().c_str());
        ImGui::Text("You have unsaved scene changes.");
        ImGui::TextDisabled("Save the scene before building a playable package.");
        ImGui::Separator();
        if (ImGui::Button("Save", ImVec2(Ui(120.0f), 0))) {
            SaveScene();
            if (EngineGUI_IsSceneDirty()) {
                MessageBoxA(GetWindowHandle(),
                            "Could not save the scene. Save it from File first, then build.",
                            "Build Current Project", MB_OK | MB_ICONWARNING);
            }
            else {
                g_showBuildUnsavedPopup = false;
                ImGui::CloseCurrentPopup();
                OpenBuildPreflight();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(Ui(120.0f), 0))) {
            g_showBuildUnsavedPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (g_showBuildPreflight)
        ImGui::OpenPopup("BuildPreflightPopup");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("BuildPreflightPopup", &g_showBuildPreflight, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Build %s?", ProjectManager::Instance().GetName().c_str());
        ImGui::TextDisabled("%s", ProjectManager::Instance().GetRootPath().c_str());
        ImGui::Separator();

        DrawPlayablePackageConfigFields();

        const std::string destReason = PlayablePackage::DestinationRefusalReason(
            ProjectManager::Instance().GetRootPath(), g_buildDest);

        ImGui::Separator();
        const bool canStart = destReason.empty() && !PlayablePackage::IsBuildRunning();
        if (!canStart)
            ImGui::BeginDisabled();
        if (ImGui::Button("Build", ImVec2(Ui(120.0f), 0))) {
            PlayablePackage::Prefs prefs;
            prefs.destination = g_buildDest;
            prefs.configuration = (g_buildConfigIndex == 1)
                ? PlayablePackage::Configuration::Release
                : PlayablePackage::Configuration::Debug;
            PlayablePackage::SavePrefs(ProjectManager::Instance().GetRootPath(), prefs);

            PlayablePackage::Request request;
            request.contentRoot = ProjectManager::Instance().GetRootPath();
            request.destination = g_buildDest;
            request.gameName = ProjectManager::Instance().GetName();
            request.configuration = prefs.configuration;
            if (PlayablePackage::StartBuild(request)) {
                g_showBuildPreflight = false;
                ImGui::CloseCurrentPopup();
                g_showBuildLog = true;
            }
        }
        if (!canStart)
            ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(Ui(120.0f), 0))) {
            g_showBuildPreflight = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Regular top-most window instead of a modal popup: it does not depend on the
    // shared popup stack, so other popups opening or closing cannot swallow it.
    if (g_showBuildLog)
    {
        const bool buildDone = PlayablePackage::IsBuildFinished();
        ImGuiWindowFlags logFlags = ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_NoDocking |
                                    ImGuiWindowFlags_NoSavedSettings;
        bool* logOpen = buildDone ? &g_showBuildLog : nullptr;
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        const bool logVisible = ImGui::Begin("Build Log##BuildLogWindow", logOpen, logFlags);
        if (logVisible) {
            if (!buildDone) {
                const std::string buildHeader =
                    "Building " + ProjectManager::Instance().GetName() + " ? Ivy Engine";
                const bool pushedLabel = editor_theme::PushLabelFont();
                ImGui::SeparatorText(buildHeader.c_str());
                if (pushedLabel)
                    editor_theme::PopLabelFont();
            }
            else if (PlayablePackage::LastBuildSucceeded())
                ImGui::Text("Build succeeded.");
            else
                ImGui::Text("Build failed.");

            std::string log;
            PlayablePackage::CopyLog(&log);
            ImGui::BeginChild("##BuildLogText", ImVec2(Ui(640.0f), Ui(280.0f)), true);
            ImGui::TextUnformatted(log.empty() ? "(no output yet)" : log.c_str());
            if (!buildDone)
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();

            if (buildDone) {
                const std::string result = PlayablePackage::LastBuildResultMessage();
                if (!result.empty())
                    ImGui::TextWrapped("%s", result.c_str());
                if (ImGui::Button("OK", ImVec2(Ui(120.0f), 0)))
                    g_showBuildLog = false;
            }
            else {
                ImGui::TextDisabled("This window stays open until MSBuild finishes.");
            }
        }
        ImGui::End();
    }
}

