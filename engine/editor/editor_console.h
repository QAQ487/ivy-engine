// editor_console.h
// Dockable editor Console panel (captures hal::dout / DebugWriteA).
#pragma once

#include "engine/editor/editor_console_log.h"
#include "libraries/nlohmann/json.hpp"

// Window title is kEditorConsoleWindowTitle in editor_docking.h (ini-stable).

EditorConsoleBuffer& EditorConsole_Buffer();
EditorConsoleFilter& EditorConsole_Filter();

bool& EditorConsole_ClearOnPlay();

// Install/remove the debug-log hook (call from EngineGUI_Initialize/Finalize).
void EditorConsole_InstallLogHook();
void EditorConsole_RemoveLogHook();

void EditorConsole_Clear();
void EditorConsole_OnPlayStarted(); // honors clear-on-play

void DrawEditorConsolePanel(bool* open);

// Prefs keys under rom/editor/ui_prefs.json (optional / cheap).
void EditorConsole_WritePrefs(nlohmann::json& j);
void EditorConsole_LoadPrefs(const nlohmann::json& j);
