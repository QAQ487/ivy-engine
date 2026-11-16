// editor_settings_categories_test.cpp
// Linux-runnable checks for the Project Settings / Preferences category registry.
//
// Build from the repo root:
//   g++ -std=c++20 -Wall -Wextra -I. -o /tmp/editor_settings_categories_test tests/editor_settings_categories_test.cpp
//   /tmp/editor_settings_categories_test

#include "engine/editor/editor_settings_categories.h"

#include <cstdio>
#include <cstring>

static int g_failed = 0;

static void Expect(bool cond, const char* msg)
{
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static void ExpectInt(int got, int expected, const char* msg)
{
    if (got != expected) {
        std::fprintf(stderr, "FAIL: %s (got %d expected %d)\n", msg, got, expected);
        ++g_failed;
    }
}

static void ExpectCategories(
    EditorSettingsWindow window,
    const char* const* ids,
    const char* const* labels,
    int count,
    const char* windowName)
{
    int gotCount = -1;
    const EditorSettingsCategory* cats = EditorSettingsCategories(window, &gotCount);
    Expect(cats != nullptr, windowName);
    ExpectInt(gotCount, count, windowName);

    if (!cats || gotCount != count)
        return;

    for (int i = 0; i < count; ++i) {
        char msg[128];
        std::snprintf(msg, sizeof(msg), "%s id[%d]", windowName, i);
        Expect(cats[i].id != nullptr && std::strcmp(cats[i].id, ids[i]) == 0, msg);
        std::snprintf(msg, sizeof(msg), "%s label[%d]", windowName, i);
        Expect(cats[i].label != nullptr && std::strcmp(cats[i].label, labels[i]) == 0, msg);
        ExpectInt(EditorSettingsCategoryIndex(window, ids[i]), i, ids[i]);
    }

    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            Expect(std::strcmp(cats[i].id, cats[j].id) != 0, "duplicate category id");
            Expect(std::strcmp(cats[i].label, cats[j].label) != 0, "duplicate category label");
        }
    }
}

static void TestProjectCategories()
{
    const char* ids[] = { "rendering", "physics", "audio", "scenes", "build" };
    const char* labels[] = { "Rendering", "Physics", "Audio", "Scenes", "Build" };
    ExpectCategories(EditorSettingsWindow::Project, ids, labels, 5, "project");
}

static void TestPreferencesCategories()
{
    const char* ids[] = { "appearance" };
    const char* labels[] = { "Appearance" };
    ExpectCategories(EditorSettingsWindow::Preferences, ids, labels, 1, "preferences");
}

static void TestUnknownIds()
{
    ExpectInt(EditorSettingsCategoryIndex(EditorSettingsWindow::Project, nullptr), -1,
        "null id");
    ExpectInt(EditorSettingsCategoryIndex(EditorSettingsWindow::Project, ""), -1,
        "empty id");
    ExpectInt(EditorSettingsCategoryIndex(EditorSettingsWindow::Project, "Rendering"), -1,
        "label is not an id");
    ExpectInt(EditorSettingsCategoryIndex(EditorSettingsWindow::Project, "RENDERING"), -1,
        "ids are case-sensitive");
    ExpectInt(EditorSettingsCategoryIndex(EditorSettingsWindow::Preferences, "rendering"), -1,
        "project id is not a preferences id");
    ExpectInt(EditorSettingsCategoryIndex(EditorSettingsWindow::Project, "appearance"), -1,
        "preferences id is not a project id");
    ExpectInt(EditorSettingsCategoryIndex(static_cast<EditorSettingsWindow>(99), "rendering"), -1,
        "invalid window");
}

static void TestNullCountIsSafe()
{
    const EditorSettingsCategory* cats =
        EditorSettingsCategories(EditorSettingsWindow::Project, nullptr);
    Expect(cats != nullptr, "null outCount still returns the table");
    Expect(EditorSettingsCategories(static_cast<EditorSettingsWindow>(-1), nullptr) == nullptr,
        "invalid window returns null");
}

static void TestClamp()
{
    ExpectInt(EditorSettingsClampCategoryIndex(EditorSettingsWindow::Project, -4), 0,
        "negative clamps to first");
    ExpectInt(EditorSettingsClampCategoryIndex(EditorSettingsWindow::Project, 0), 0,
        "zero stays");
    ExpectInt(EditorSettingsClampCategoryIndex(EditorSettingsWindow::Project, 4), 4,
        "last project index stays");
    ExpectInt(EditorSettingsClampCategoryIndex(EditorSettingsWindow::Project, 40), 4,
        "past-the-end clamps to last");
    ExpectInt(EditorSettingsClampCategoryIndex(EditorSettingsWindow::Preferences, 3), 0,
        "preferences clamps to its only page");
    ExpectInt(EditorSettingsClampCategoryIndex(static_cast<EditorSettingsWindow>(99), 1), 0,
        "invalid window clamps to 0");
}

int main()
{
    TestProjectCategories();
    TestPreferencesCategories();
    TestUnknownIds();
    TestNullCountIsSafe();
    TestClamp();

    if (g_failed != 0) {
        std::fprintf(stderr, "%d failure(s)\n", g_failed);
        return 1;
    }
    std::printf("editor_settings_categories_test: ok\n");
    return 0;
}
