// editor_docking.h
// ImGui docking host (PassthruCentralNode) + per-project layout persistence.
#pragma once

#include "engine/editor/editor_docking_math.h"

#include <string>

// Window titles / ImGui IDs — keep stable for .ini persistence.
inline constexpr const char* kEditorHierarchyWindowTitle = "Scene Hierarchy";
inline constexpr const char* kEditorPropertiesWindowTitle = "Properties";
inline constexpr const char* kEditorAssetBrowserWindowTitle = "Asset Browser";

struct EditorPanelVisibility {
    bool hierarchy = true;
    bool properties = true;
    bool assetBrowser = true;
};

// Call once after CreateContext: DockingEnable, IniFilename=nullptr (no cwd imgui.ini).
void EditorDocking_ConfigureImGuiIO();

// Per-project layout load/save via UTF-8 path helpers + memory ini APIs.
// Open/close may be requested mid-frame; apply after NewFrame via BeginFrame.
void EditorDocking_OnProjectOpened(const std::string& projectRootUtf8);
void EditorDocking_OnProjectClosed();
void EditorDocking_BeginFrame();
void EditorDocking_FlushIniIfNeeded();
void EditorDocking_RequestResetLayout();

// Host dockspace below menu+toolbar. Captures central-node client rect.
void EditorDocking_DrawHost();

// Last captured central node in ImGui client coordinates (after DrawHost).
const EditorClientRect& EditorDocking_GetCentralNodeClientRect();
bool EditorDocking_HasCentralNode();

EditorPanelVisibility& EditorDocking_PanelVisibility();

// True when ImGui should block game picking/gizmos (panel/tab/menu hover).
// Passthru central node returns false so the D3D scene stays interactive.
bool EditorDocking_WantCaptureMouseForGame();
