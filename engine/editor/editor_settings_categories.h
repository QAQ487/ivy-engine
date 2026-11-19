// editor_settings_categories.h
// Pure category registry for Project Settings and Preferences.
// Header-only so Linux unit tests can compile it without the editor TUs.
//
// Ids are stable and case-sensitive. Labels are the page titles shown in the
// left-hand list. Order is the order of that list.
#pragma once

#include <cstring>

enum class EditorSettingsWindow : int {
    Project = 0,
    Preferences = 1,
};

struct EditorSettingsCategory {
    const char* id = nullptr;
    const char* label = nullptr;
};

inline const EditorSettingsCategory* EditorSettingsCategories(
    EditorSettingsWindow window, int* outCount)
{
    static constexpr EditorSettingsCategory kProject[] = {
        { "rendering", "Rendering" },
        { "physics", "Physics" },
        { "audio", "Audio" },
        { "input", "Input" },
        { "scenes", "Scenes" },
        { "build", "Build" },
    };
    static constexpr EditorSettingsCategory kPreferences[] = {
        { "appearance", "Appearance" },
    };

    const EditorSettingsCategory* table = nullptr;
    int count = 0;
    if (window == EditorSettingsWindow::Project) {
        table = kProject;
        count = static_cast<int>(sizeof(kProject) / sizeof(kProject[0]));
    }
    else if (window == EditorSettingsWindow::Preferences) {
        table = kPreferences;
        count = static_cast<int>(sizeof(kPreferences) / sizeof(kPreferences[0]));
    }

    if (outCount)
        *outCount = count;
    return table;
}

inline int EditorSettingsCategoryIndex(EditorSettingsWindow window, const char* id)
{
    if (!id || !id[0])
        return -1;
    int count = 0;
    const EditorSettingsCategory* cats = EditorSettingsCategories(window, &count);
    if (!cats)
        return -1;
    for (int i = 0; i < count; ++i) {
        if (cats[i].id && std::strcmp(cats[i].id, id) == 0)
            return i;
    }
    return -1;
}

inline int EditorSettingsClampCategoryIndex(EditorSettingsWindow window, int index)
{
    int count = 0;
    EditorSettingsCategories(window, &count);
    if (count <= 0)
        return 0;
    if (index < 0)
        return 0;
    if (index >= count)
        return count - 1;
    return index;
}
