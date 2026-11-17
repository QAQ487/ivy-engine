// editor_settings_windows.cpp
// Dockable Project Settings and Preferences shells.
// Page bodies are the existing settings draw functions; this file only hosts them.

#include "editor_gui_common_includes.h"
#include "engine/editor/editor_settings_categories.h"

#include <cstring>

namespace {

bool g_projectOpen = false;
bool g_preferencesOpen = false;
bool g_focusProject = false;
bool g_focusPreferences = false;
int g_projectCategory = 0;
int g_preferencesCategory = 0;

std::string g_buildAppliedDest;
int g_buildAppliedConfig = -1;
bool g_buildAppliedValid = false;
bool g_buildPageWasVisible = false;

void RememberBuildBuffers()
{
    g_buildAppliedDest = g_buildDest;
    g_buildAppliedConfig = g_buildConfigIndex;
    g_buildAppliedValid = true;
}

void LoadBuildPrefsIntoBuffers()
{
    if (!ProjectManager::Instance().IsOpen())
        return;
    PlayablePackage::Prefs prefs;
    PlayablePackage::LoadPrefs(ProjectManager::Instance().GetRootPath(), &prefs);
    SetBuildDestBuffer(prefs.destination);
    g_buildConfigIndex =
        (prefs.configuration == PlayablePackage::Configuration::Release) ? 1 : 0;
    RememberBuildBuffers();
}

void SaveBuildPrefsFromBuffersIfChanged()
{
    if (!ProjectManager::Instance().IsOpen() || !g_buildAppliedValid)
        return;
    if (g_buildAppliedDest == g_buildDest && g_buildAppliedConfig == g_buildConfigIndex)
        return;
    if (g_showBuildPreflight)
        return;

    PlayablePackage::Prefs prefs;
    prefs.destination = g_buildDest;
    prefs.configuration = (g_buildConfigIndex == 1)
        ? PlayablePackage::Configuration::Release
        : PlayablePackage::Configuration::Debug;
    PlayablePackage::SavePrefs(ProjectManager::Instance().GetRootPath(), prefs);
    RememberBuildBuffers();
}

void DrawProjectBuildPage(bool justShown)
{
    static bool preflightWasOpen = false;
    if (g_showBuildPreflight) {
        preflightWasOpen = true;
        ImGui::TextUnformatted("Playable Package");
        ImGui::TextDisabled("Finish or cancel Build Current Project to edit these settings.");
        return;
    }
    // Opening this page, or closing the build dialog, reloads saved prefs so a
    // cancelled dialog does not get written from the shared buffers.
    if (justShown || preflightWasOpen)
        LoadBuildPrefsIntoBuffers();
    preflightWasOpen = false;

    ImGui::TextUnformatted("Playable Package");
    ImGui::TextDisabled("Same output folder and configuration as File > Build Current Project.");
    ImGui::Separator();
    DrawPlayablePackageConfigFields();
    if (!ImGui::IsAnyItemActive())
        SaveBuildPrefsFromBuffersIfChanged();

    ImGui::Spacing();
    const bool canBuild = !g_isPlaying && !IsSceneLoadBusy() &&
        ProjectManager::Instance().IsOpen() && !PlayablePackage::IsBuildRunning();
    if (!canBuild)
        ImGui::BeginDisabled();
    if (ImGui::Button("Build Current Project...")) {
        SaveBuildPrefsFromBuffersIfChanged();
        RequestBuildCurrentProject();
    }
    if (!canBuild)
        ImGui::EndDisabled();
}

void DrawProjectSettingsPage(const char* id, bool buildJustShown)
{
    if (!id)
        return;
    if (std::strcmp(id, "rendering") == 0) {
        DrawRenderingSettingsBody();
        return;
    }

    const bool lock = g_isPlaying || IsSceneLoadBusy();
    if (lock)
        ImGui::BeginDisabled();
    if (std::strcmp(id, "physics") == 0)
        DrawProjectGravitySettings();
    else if (std::strcmp(id, "audio") == 0)
        DrawProjectAudioSettings();
    else if (std::strcmp(id, "scenes") == 0)
        DrawProjectDefaultSceneSettings();
    else if (std::strcmp(id, "build") == 0)
        DrawProjectBuildPage(buildJustShown);
    if (lock)
        ImGui::EndDisabled();
}

void DrawPreferencesAppearancePage()
{
    ImGui::TextUnformatted("UI Scale");
    ImGui::Separator();
    for (int percent : kUiZoomPercents) {
        char label[32];
        snprintf(label, sizeof(label), "%d%%##zoom", percent);
        if (ImGui::RadioButton(label, g_uiZoomPercent == percent)) {
            ApplyUiZoom(percent);
            SaveUiPrefs();
            ProjectManager::Instance().SetEditorUiZoomPercent(percent);
        }
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("UI Style");
    ImGui::Separator();
    for (int i = 0; i < static_cast<int>(editor_theme::StylePreset::Count); ++i) {
        const auto preset = static_cast<editor_theme::StylePreset>(i);
        const char* name = editor_theme::StylePresetName(preset);
        char label[64];
        snprintf(label, sizeof(label), "%s##style", name);
        if (ImGui::RadioButton(label, g_uiStylePreset == preset)) {
            ApplyEditorTheme(preset, g_uiColorScheme); // reapplies effective scale
            SaveUiPrefs();
            ProjectManager::Instance().SetEditorUiStylePreset(name);
        }
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Color Scheme");
    ImGui::Separator();
    for (int i = 0; i < static_cast<int>(editor_theme::ColorScheme::Count); ++i) {
        const auto scheme = static_cast<editor_theme::ColorScheme>(i);
        const char* name = editor_theme::ColorSchemeName(scheme);
        char label[64];
        snprintf(label, sizeof(label), "%s##scheme", name);
        if (ImGui::RadioButton(label, g_uiColorScheme == scheme)) {
            ApplyEditorTheme(g_uiStylePreset, scheme); // reapplies effective scale
            SaveUiPrefs();
            ProjectManager::Instance().SetEditorUiColorScheme(name);
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Editor fonts are fixed: IBM Plex Sans JP, IBM Plex Mono, and Font Awesome.");
    ImGui::TextDisabled("Effective scale = UI Scale × monitor DPI (applied once; no compounding).");
    ImGui::TextDisabled("UI scale, style, and color scheme are saved to rom/editor/ui_prefs.json.");
    ImGui::TextDisabled("Save Project also copies them into project.json.");
}

void DrawPreferencesPage(const char* id)
{
    if (id && std::strcmp(id, "appearance") == 0)
        DrawPreferencesAppearancePage();
}

void DrawCategoryColumn(EditorSettingsWindow window, int* categoryIndex)
{
    int count = 0;
    const EditorSettingsCategory* cats = EditorSettingsCategories(window, &count);
    if (categoryIndex)
        *categoryIndex = EditorSettingsClampCategoryIndex(window, *categoryIndex);

    ImGui::BeginChild("##SettingsCategories", ImVec2(Ui(180.0f), 0.0f), ImGuiChildFlags_Borders);
    if (cats && categoryIndex) {
        for (int i = 0; i < count; ++i) {
            if (ImGui::Selectable(cats[i].label, *categoryIndex == i))
                *categoryIndex = i;
        }
    }
    ImGui::EndChild();
}

bool BeginSettingsWindow(const char* title, bool* open, bool* focus)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(Ui(760.0f), Ui(520.0f)), ImGuiCond_FirstUseEver);
    if (viewport)
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    if (focus && *focus) {
        ImGui::SetNextWindowFocus();
        *focus = false;
    }
    if (!ImGui::Begin(title, open)) {
        ImGui::End();
        return false;
    }
    return true;
}

void SaveProjectSettingsFile()
{
    if (ProjectManager::Instance().IsOpen())
        ProjectManager::Instance().SaveProjectFile(nullptr);
}

} // namespace

bool& EditorSettings_ProjectWindowOpen()
{
    return g_projectOpen;
}

bool& EditorSettings_PreferencesWindowOpen()
{
    return g_preferencesOpen;
}

void EditorSettings_OpenProjectSettings()
{
    g_projectOpen = true;
    g_focusProject = true;
}

void EditorSettings_OpenPreferences()
{
    g_preferencesOpen = true;
    g_focusPreferences = true;
}

void DrawProjectSettingsWindow()
{
    static bool wasOpen = false;
    if (!g_projectOpen) {
        if (wasOpen)
            SaveProjectSettingsFile();
        if (g_buildPageWasVisible) {
            SaveBuildPrefsFromBuffersIfChanged();
            g_buildPageWasVisible = false;
        }
        wasOpen = false;
        return;
    }

    const bool visible = BeginSettingsWindow("Project Settings", &g_projectOpen, &g_focusProject);
    if (visible) {
        DrawCategoryColumn(EditorSettingsWindow::Project, &g_projectCategory);
        ImGui::SameLine();
        ImGui::BeginChild("##SettingsPage", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
        int count = 0;
        const EditorSettingsCategory* cats =
            EditorSettingsCategories(EditorSettingsWindow::Project, &count);
        g_projectCategory = EditorSettingsClampCategoryIndex(
            EditorSettingsWindow::Project, g_projectCategory);
        const bool buildVisible = cats && g_projectCategory >= 0 && g_projectCategory < count &&
            cats[g_projectCategory].id &&
            std::strcmp(cats[g_projectCategory].id, "build") == 0;
        if (g_buildPageWasVisible && !buildVisible)
            SaveBuildPrefsFromBuffersIfChanged();
        if (cats && g_projectCategory >= 0 && g_projectCategory < count)
            DrawProjectSettingsPage(cats[g_projectCategory].id, buildVisible && !g_buildPageWasVisible);
        g_buildPageWasVisible = buildVisible;
        ImGui::EndChild();
        ImGui::End();
    }
    else if (g_buildPageWasVisible) {
        SaveBuildPrefsFromBuffersIfChanged();
        g_buildPageWasVisible = false;
    }

    if (!g_projectOpen)
        SaveProjectSettingsFile();
    wasOpen = g_projectOpen;
}

void DrawPreferencesWindow()
{
    if (!g_preferencesOpen)
        return;

    const bool visible = BeginSettingsWindow("Preferences", &g_preferencesOpen, &g_focusPreferences);
    if (!visible)
        return;

    DrawCategoryColumn(EditorSettingsWindow::Preferences, &g_preferencesCategory);
    ImGui::SameLine();
    ImGui::BeginChild("##SettingsPage", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    int count = 0;
    const EditorSettingsCategory* cats =
        EditorSettingsCategories(EditorSettingsWindow::Preferences, &count);
    g_preferencesCategory = EditorSettingsClampCategoryIndex(
        EditorSettingsWindow::Preferences, g_preferencesCategory);
    if (cats && g_preferencesCategory >= 0 && g_preferencesCategory < count)
        DrawPreferencesPage(cats[g_preferencesCategory].id);
    ImGui::EndChild();
    ImGui::End();
}
