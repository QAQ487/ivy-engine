// camera_orientation_gizmo.cpp
// Editor camera orientation gizmo widget.

#include "camera_orientation_gizmo.h"

#ifndef GAME_RUNTIME
#include "camera.h"
#include "debug_camera.h"
#include "engine/core/engine_gui.h"
#include "engine/editor/editor_docking.h"
#include "engine/input/mouse.h"
#include "imgui/imgui.h"

#include <DirectXMath.h>
#include <algorithm>
#include <cfloat>
#include <cmath>

using namespace DirectX;

namespace {

constexpr float kMargin = 16.0f;
constexpr float kGizmoRadius = 45.0f;
constexpr float kArmLength = 32.0f;

struct AxisInfo {
    DebugCameraViewAxis axis;
    XMFLOAT3 dir;
    const char* label;      // "X", "Y", "Z" for positive, nullptr for negative
    const char* tooltip;    // Human-readable view description
    ImU32 color;            // Axis line and positive handle color
    ImU32 darkColor;        // Subdued color for negative handle
    bool isPositive;
};

static const AxisInfo kAxes[6] = {
    // Positive axes (+X, +Y, +Z)
    { DebugCameraViewAxis::PosX, {  1.0f,  0.0f,  0.0f }, "X", "+X (Right)",
      IM_COL32(235, 60, 60, 255), IM_COL32(180, 50, 50, 220), true },
    { DebugCameraViewAxis::PosY, {  0.0f,  1.0f,  0.0f }, "Y", "+Y (Top)",
      IM_COL32(60, 205, 60, 255), IM_COL32(50, 160, 50, 220), true },
    { DebugCameraViewAxis::PosZ, {  0.0f,  0.0f,  1.0f }, "Z", "+Z (Front)",
      IM_COL32(50, 135, 235, 255), IM_COL32(40, 105, 195, 220), true },

    // Negative axes (-X, -Y, -Z)
    { DebugCameraViewAxis::NegX, { -1.0f,  0.0f,  0.0f }, nullptr, "-X (Left)",
      IM_COL32(235, 60, 60, 255), IM_COL32(180, 50, 50, 200), false },
    { DebugCameraViewAxis::NegY, {  0.0f, -1.0f,  0.0f }, nullptr, "-Y (Bottom)",
      IM_COL32(60, 205, 60, 255), IM_COL32(50, 160, 50, 200), false },
    { DebugCameraViewAxis::NegZ, {  0.0f,  0.0f, -1.0f }, nullptr, "-Z (Back)",
      IM_COL32(50, 135, 235, 255), IM_COL32(40, 105, 195, 200), false }
};

struct ProjectedAxis {
    int index = 0;
    float screenX = 0.0f;
    float screenY = 0.0f;
    float depthZ = 0.0f; // View-space Z: larger = farther into screen, smaller/negative = closer to viewer
};

bool GetGizmoCenter(float& outX, float& outY) {
    if (EngineGUI_IsPlaying() || !EngineGUI_IsEnabled() || EngineGUI_IsUiEditMode())
        return false;
    if (!EditorDocking_HasCentralNode())
        return false;

    const EditorClientRect& c = EditorDocking_GetCentralNodeClientRect();
    if (c.w < 120.0f || c.h < 120.0f)
        return false;

    outX = c.x + c.w - kMargin - kGizmoRadius;
    outY = c.y + kMargin + kGizmoRadius;
    return true;
}

} // namespace

namespace CameraOrientationGizmo {

bool IsInteracting() {
    if(DebugCamera_Is2DView()) return false;
    if(EditorDocking_WantCaptureMouseForGame()) return false;
    float cx = 0.0f, cy = 0.0f;
    if (!GetGizmoCenter(cx, cy))
        return false;

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float dx = mouse.x - cx;
    const float dy = mouse.y - cy;
    return (dx * dx + dy * dy) <= (kGizmoRadius + 4.0f) * (kGizmoRadius + 4.0f);
}

void Draw() {
    float centerX = 0.0f, centerY = 0.0f;
    if (!GetGizmoCenter(centerX, centerY))
        return;

    const ImVec2 center(centerX, centerY);

    // Current camera view matrix
    Camera& cam = GetCamera();
    const XMMATRIX view = cam.GetViewMatrix();

    // Project each cardinal axis into 2D view space
    ProjectedAxis proj[6];
    for (int i = 0; i < 6; ++i) {
        const XMVECTOR vWorldDir = XMVectorSet(kAxes[i].dir.x, kAxes[i].dir.y, kAxes[i].dir.z, 0.0f);
        const XMVECTOR vViewDir = XMVector3TransformNormal(vWorldDir, view);
        XMFLOAT3 viewDir;
        XMStoreFloat3(&viewDir, vViewDir);

        proj[i].index = i;
        proj[i].screenX = centerX + viewDir.x * kArmLength;
        proj[i].screenY = centerY - viewDir.y * kArmLength; // Y inverted for screen coordinates
        proj[i].depthZ = viewDir.z;
    }

    // Hit testing on the 6 endpoints
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    int hoveredIndex = -1;
    float bestDepth = FLT_MAX;

    for (int i = 0; i < 6; ++i) {
        const float dx = mouse.x - proj[i].screenX;
        const float dy = mouse.y - proj[i].screenY;
        const float dist = sqrtf(dx * dx + dy * dy);
        const float hitRadius = kAxes[i].isPositive ? 11.5f : 8.5f;

        if (!EditorDocking_WantCaptureMouseForGame() && dist <= hitRadius) {
            // Prioritize endpoints closer to viewer in depth
            if (proj[i].depthZ < bestDepth) {
                bestDepth = proj[i].depthZ;
                hoveredIndex = i;
            }
        }
    }

    const float centerDx = mouse.x - centerX;
    const float centerDy = mouse.y - centerY;
    const bool isOverDisk = (centerDx * centerDx + centerDy * centerDy) <= (kGizmoRadius * kGizmoRadius);

    if (!EditorDocking_WantCaptureMouseForGame() && (isOverDisk || hoveredIndex >= 0)) {
        ImGui::GetIO().WantCaptureMouse = true;
    }

    if (hoveredIndex >= 0) {
        ImGui::SetTooltip("%s", kAxes[hoveredIndex].tooltip);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            DebugCamera_SnapToAxis(kAxes[hoveredIndex].axis, 0.25f);
        }
    }

    // Sort axes from back to front (painter's algorithm)
    ProjectedAxis sorted[6];
    for (int i = 0; i < 6; ++i) sorted[i] = proj[i];
    std::sort(sorted, sorted + 6, [](const ProjectedAxis& a, const ProjectedAxis& b) {
        return a.depthZ > b.depthZ; // Farthest (most positive depth) first
    });

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Background disc
    dl->AddCircleFilled(center, kGizmoRadius, IM_COL32(30, 32, 36, 175), 32);
    dl->AddCircle(center, kGizmoRadius, IM_COL32(80, 85, 95, 180), 32, 1.2f);

    // Center pivot anchor dot
    dl->AddCircleFilled(center, 2.5f, IM_COL32(130, 135, 145, 180));

    // Render 6 axis endpoints back to front
    for (int i = 0; i < 6; ++i) {
        const int idx = sorted[i].index;
        const auto& axis = kAxes[idx];
        const ImVec2 pt(sorted[i].screenX, sorted[i].screenY);
        const bool isHovered = (idx == hoveredIndex);

        if (axis.isPositive) {
            // Positive axis: solid colored line, larger bubble with letter label
            dl->AddLine(center, pt, axis.color, 2.5f);

            const float r = isHovered ? 10.0f : 8.5f;
            dl->AddCircleFilled(pt, r, axis.color, 20);
            dl->AddCircle(pt, r, IM_COL32(255, 255, 255, isHovered ? 255 : 180), 20, 1.2f);

            if (isHovered) {
                dl->AddCircle(pt, r + 2.5f, IM_COL32(255, 255, 255, 230), 20, 2.0f);
            }

            const ImVec2 textSize = ImGui::CalcTextSize(axis.label);
            const ImVec2 textPos(pt.x - textSize.x * 0.5f, pt.y - textSize.y * 0.5f);
            dl->AddText(textPos, IM_COL32(255, 255, 255, 255), axis.label);
        } else {
            // Negative axis: subtle thinner line and distinct smaller bead
            dl->AddLine(center, pt, axis.darkColor, 1.2f);

            const float r = isHovered ? 6.5f : 4.5f;
            dl->AddCircleFilled(pt, r, axis.darkColor, 16);
            dl->AddCircle(pt, r, IM_COL32(210, 215, 225, isHovered ? 255 : 150), 16, 1.0f);

            if (isHovered) {
                dl->AddCircle(pt, r + 2.5f, IM_COL32(255, 255, 255, 230), 16, 1.8f);
            }
        }
    }
}

} // namespace CameraOrientationGizmo
#endif // !GAME_RUNTIME
