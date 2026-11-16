// editor_layout.h
// Extracted editor chrome / viewport layout helpers.
#pragma once

#include "imgui/imgui.h"

struct EditorLayoutMetrics {
    float topChrome = 0.0f;
    float leftWidth = 0.0f;
    float rightWidth = 0.0f;
    float bottomHeight = 0.0f;
    float sidePanelH = 0.0f;
    float viewportX = 0.0f;
    float viewportY = 0.0f;
    float viewportW = 0.0f;
    float viewportH = 0.0f;
};

float Ui(float px);
float EditorMenuBarHeight();
float EditorToolbarHeight();
float EditorTopChrome();
EditorLayoutMetrics ComputeEditorLayout(float screenW, float screenH);

ImVec2 ToolbarWindowPadding();
ImVec2 ToolbarFramePadding();
