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

// Unity-style property row: the label sits in a left column (40% of the panel,
// clipped with a tooltip when too long) and the next widget fills the rest of
// the row. Declare it in a scope around one widget and give that widget the
// hidden id "##v":
//     { InspectorRow row("Size X"); ImGui::DragFloat("##v", &x); }
class InspectorRow {
public:
    explicit InspectorRow(const char* label);
    ~InspectorRow();
    InspectorRow(const InspectorRow&) = delete;
    InspectorRow& operator=(const InspectorRow&) = delete;
};

// Equal-width toggle buttons laid out in `columns` per row (wraps, never wider
// than the panel). The button at *selected is highlighted. Returns true when
// the selection changed.
bool InspectorToggleGrid(const char* const* labels, int count, int* selected, int columns);

ImVec2 ToolbarWindowPadding();
ImVec2 ToolbarFramePadding();
