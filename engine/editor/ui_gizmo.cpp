// ui_gizmo.cpp
// Editor gizmo for UI element transforms.

#include "ui_gizmo.h"

#include "engine/core/engine_gui.h"
#include "engine/editor/editor_undo.h"
#include "engine/input/mouse.h"
#include "engine/ui/scene_canvas.h"
#include "engine/ui/ui_bar.h"
#include "engine/ui/ui_button.h"
#include "engine/ui/ui_element.h"
#include "engine/ui/ui_sprite.h"
#include "imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

enum class GizmoHandle {
    None = 0,
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left,
    Move,
    MoveArrowX,
    MoveArrowY
};

constexpr float kHandleHalfSize = 5.0f;
constexpr float kMinDesignSize = 8.0f;
constexpr float kArrowLength = 56.0f;
constexpr float kArrowHeadSize = 10.0f;
constexpr float kArrowHitThickness = 10.0f;
constexpr ImU32 kOutlineColor = IM_COL32(255, 255, 0, 255);
constexpr ImU32 kHandleColor = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kHandleHoverColor = IM_COL32(120, 180, 255, 255);
constexpr ImU32 kAnchorColor = IM_COL32(255, 128, 0, 255);
constexpr ImU32 kArrowXColor = IM_COL32(220, 70, 70, 255);
constexpr ImU32 kArrowYColor = IM_COL32(80, 200, 90, 255);
constexpr ImU32 kArrowXHoverColor = IM_COL32(255, 120, 120, 255);
constexpr ImU32 kArrowYHoverColor = IM_COL32(130, 255, 140, 255);

bool g_interacting = false;
GizmoHandle g_activeHandle = GizmoHandle::None;
GizmoHandle g_hoverHandle = GizmoHandle::None;
UIScreenRect g_startRect = {};
DirectX::XMFLOAT2 g_startPosition = {};
DirectX::XMFLOAT2 g_startSize = {};
float g_dragStartLocalX = 0.0f;
float g_dragStartLocalY = 0.0f;
bool g_prevLDown = false;
std::string g_beforeJson;
UIElement* g_dragElement = nullptr;

void ViewportLocalToWindow(float& x, float& y) {
    float vx = 0.0f, vy = 0.0f, vw = 0.0f, vh = 0.0f;
    EngineGUI_GetGameViewportRect(vx, vy, vw, vh);
    x += vx;
    y += vy;
}

bool IsResizable(UIElement* element) {
    if(auto* sprite=dynamic_cast<UISprite*>(element); sprite && sprite->HasExplicitPivot() && sprite->GetRotation()!=0) return false;
    return dynamic_cast<UIButton*>(element) != nullptr
        || dynamic_cast<UISprite*>(element) != nullptr
        || dynamic_cast<UIBar*>(element) != nullptr;
}

void GetElementSize(UIElement* element, float& width, float& height) {
    width = 0.0f;
    height = 0.0f;
    if (auto* button = dynamic_cast<UIButton*>(element)) {
        width = button->GetWidth();
        height = button->GetHeight();
    } else if (auto* sprite = dynamic_cast<UISprite*>(element)) {
        width = sprite->GetWidth();
        height = sprite->GetHeight();
    } else if (auto* bar = dynamic_cast<UIBar*>(element)) {
        width = static_cast<float>(bar->GetBarWidth());
        height = static_cast<float>(bar->GetBarHeight());
    }
}

void SetElementSize(UIElement* element, float width, float height) {
    width = (std::max)(width, kMinDesignSize);
    height = (std::max)(height, kMinDesignSize);
    if (auto* button = dynamic_cast<UIButton*>(element)) {
        button->SetSize(width, height);
    } else if (auto* sprite = dynamic_cast<UISprite*>(element)) {
        sprite->SetSize(width, height);
    } else if (auto* bar = dynamic_cast<UIBar*>(element)) {
        bar->SetBarSize(static_cast<int>(std::lround(width)), static_cast<int>(std::lround(height)));
    }
}

float DistancePointToSegment(float px, float py, float ax, float ay, float bx, float by) {
    const float abx = bx - ax;
    const float aby = by - ay;
    const float apx = px - ax;
    const float apy = py - ay;
    const float abLenSq = abx * abx + aby * aby;
    if (abLenSq <= 1e-4f)
        return std::sqrt(apx * apx + apy * apy);

    float t = (apx * abx + apy * aby) / abLenSq;
    t = (std::max)(0.0f, (std::min)(1.0f, t));
    const float cx = ax + abx * t;
    const float cy = ay + aby * t;
    const float dx = px - cx;
    const float dy = py - cy;
    return std::sqrt(dx * dx + dy * dy);
}

bool HitTestTriangle(float px, float py, float x0, float y0, float x1, float y1, float x2, float y2) {
    const float d1 = (px - x1) * (y0 - y1) - (x0 - x1) * (py - y1);
    const float d2 = (px - x2) * (y1 - y2) - (x1 - x2) * (py - y2);
    const float d3 = (px - x0) * (y2 - y0) - (x2 - x0) * (py - y0);
    const bool hasNeg = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
    const bool hasPos = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);
    return !(hasNeg && hasPos);
}

GizmoHandle HitTestMoveArrow(float mouseX, float mouseY, float pivotX, float pivotY) {
    const float xEnd = pivotX + kArrowLength;
    const float yEnd = pivotY + kArrowLength;

    if (DistancePointToSegment(mouseX, mouseY, pivotX, pivotY, xEnd, pivotY) <= kArrowHitThickness
        || HitTestTriangle(mouseX, mouseY,
            xEnd, pivotY,
            xEnd - kArrowHeadSize, pivotY - kArrowHeadSize * 0.5f,
            xEnd - kArrowHeadSize, pivotY + kArrowHeadSize * 0.5f)) {
        return GizmoHandle::MoveArrowX;
    }

    if (DistancePointToSegment(mouseX, mouseY, pivotX, pivotY, pivotX, yEnd) <= kArrowHitThickness
        || HitTestTriangle(mouseX, mouseY,
            pivotX, yEnd,
            pivotX - kArrowHeadSize * 0.5f, yEnd - kArrowHeadSize,
            pivotX + kArrowHeadSize * 0.5f, yEnd - kArrowHeadSize)) {
        return GizmoHandle::MoveArrowY;
    }

    return GizmoHandle::None;
}

GizmoHandle HitTestHandle(float mouseX, float mouseY, const UIScreenRect& rect, bool resizable, const DirectX::XMFLOAT2& pivot) {
    const float pivotX = pivot.x;
    const float pivotY = pivot.y;

    if (GizmoHandle arrow = HitTestMoveArrow(mouseX, mouseY, pivotX, pivotY);
        arrow != GizmoHandle::None) {
        return arrow;
    }

    const float hs = kHandleHalfSize;
    const float left = rect.left;
    const float right = rect.right;
    const float top = rect.top;
    const float bottom = rect.bottom;
    const float midX = (left + right) * 0.5f;
    const float midY = (top + bottom) * 0.5f;

    auto isNear = [&](float x, float y) {
        return std::fabs(mouseX - x) <= hs && std::fabs(mouseY - y) <= hs;
    };

    if (resizable) {
        if (isNear(left, top)) return GizmoHandle::TopLeft;
        if (isNear(midX, top)) return GizmoHandle::Top;
        if (isNear(right, top)) return GizmoHandle::TopRight;
        if (isNear(right, midY)) return GizmoHandle::Right;
        if (isNear(right, bottom)) return GizmoHandle::BottomRight;
        if (isNear(midX, bottom)) return GizmoHandle::Bottom;
        if (isNear(left, bottom)) return GizmoHandle::BottomLeft;
        if (isNear(left, midY)) return GizmoHandle::Left;
    }

    if (mouseX >= left && mouseX <= right && mouseY >= top && mouseY <= bottom)
        return GizmoHandle::Move;

    return GizmoHandle::None;
}

void DrawMoveArrow(ImDrawList* drawList, float ox, float oy, bool horizontal, bool hovered) {
    const ImU32 color = horizontal
        ? (hovered ? kArrowXHoverColor : kArrowXColor)
        : (hovered ? kArrowYHoverColor : kArrowYColor);
    const float thickness = hovered ? 3.0f : 2.0f;

    if (horizontal) {
        const float xEnd = ox + kArrowLength;
        drawList->AddLine(ImVec2(ox, oy), ImVec2(xEnd, oy), color, thickness);
        drawList->AddTriangleFilled(
            ImVec2(xEnd, oy),
            ImVec2(xEnd - kArrowHeadSize, oy - kArrowHeadSize * 0.5f),
            ImVec2(xEnd - kArrowHeadSize, oy + kArrowHeadSize * 0.5f),
            color);
    } else {
        const float yEnd = oy + kArrowLength;
        drawList->AddLine(ImVec2(ox, oy), ImVec2(ox, yEnd), color, thickness);
        drawList->AddTriangleFilled(
            ImVec2(ox, yEnd),
            ImVec2(ox - kArrowHeadSize * 0.5f, yEnd - kArrowHeadSize),
            ImVec2(ox + kArrowHeadSize * 0.5f, yEnd - kArrowHeadSize),
            color);
    }
}

void DrawHandle(ImDrawList* drawList, float x, float y, bool hovered) {
    const ImU32 color = hovered ? kHandleHoverColor : kHandleColor;
    drawList->AddRectFilled(
        ImVec2(x - kHandleHalfSize, y - kHandleHalfSize),
        ImVec2(x + kHandleHalfSize, y + kHandleHalfSize),
        color);
    drawList->AddRect(
        ImVec2(x - kHandleHalfSize, y - kHandleHalfSize),
        ImVec2(x + kHandleHalfSize, y + kHandleHalfSize),
        kOutlineColor);
}

void ApplyResize(UIElement* element, GizmoHandle handle, float mouseX, float mouseY) {
    const DirectX::XMFLOAT2 layoutScale = UIGetLayoutScale();
    if (layoutScale.x <= 0.0f || layoutScale.y <= 0.0f)
        return;

    float left = g_startRect.left;
    float top = g_startRect.top;
    float right = g_startRect.right;
    float bottom = g_startRect.bottom;

    switch (handle) {
    case GizmoHandle::TopLeft:
        left = mouseX;
        top = mouseY;
        break;
    case GizmoHandle::Top:
        top = mouseY;
        break;
    case GizmoHandle::TopRight:
        right = mouseX;
        top = mouseY;
        break;
    case GizmoHandle::Right:
        right = mouseX;
        break;
    case GizmoHandle::BottomRight:
        right = mouseX;
        bottom = mouseY;
        break;
    case GizmoHandle::Bottom:
        bottom = mouseY;
        break;
    case GizmoHandle::BottomLeft:
        left = mouseX;
        bottom = mouseY;
        break;
    case GizmoHandle::Left:
        left = mouseX;
        break;
    default:
        return;
    }

    float width = (right - left) / layoutScale.x;
    float height = (bottom - top) / layoutScale.y;
    width = (std::max)(width, kMinDesignSize);
    height = (std::max)(height, kMinDesignSize);

    if (handle == GizmoHandle::TopLeft || handle == GizmoHandle::Left || handle == GizmoHandle::BottomLeft)
        left = right - width * layoutScale.x;
    else
        right = left + width * layoutScale.x;

    if (handle == GizmoHandle::TopLeft || handle == GizmoHandle::Top || handle == GizmoHandle::TopRight)
        top = bottom - height * layoutScale.y;
    else
        bottom = top + height * layoutScale.y;

    DirectX::XMFLOAT2 position = g_startPosition;
    position.x += (left - g_startRect.left) / layoutScale.x;
    position.y += (top - g_startRect.top) / layoutScale.y;
    if (element->HasExplicitPivot()) {
        const DirectX::XMFLOAT2 pivot = element->GetPivot();
        position.x += pivot.x * (width - g_startSize.x);
        position.y += (1.0f - pivot.y) * (height - g_startSize.y);
    }
    element->SetPosition(position);
    SetElementSize(element, width, height);
}

void ApplyMove(UIElement* element, GizmoHandle handle, float localMouseX, float localMouseY) {
    const DirectX::XMFLOAT2 delta = ScreenDeltaToDesignDelta(
        localMouseX - g_dragStartLocalX,
        localMouseY - g_dragStartLocalY);

    DirectX::XMFLOAT2 position = g_startPosition;
    if (handle == GizmoHandle::Move) {
        position.x += delta.x;
        position.y += delta.y;
    } else if (handle == GizmoHandle::MoveArrowX) {
        position.x += delta.x;
    } else if (handle == GizmoHandle::MoveArrowY) {
        position.y += delta.y;
    } else {
        return;
    }

    element->SetPosition(position);
    EngineGUI_MarkSceneDirty();
}

} // namespace

namespace UiGizmo {

bool IsInteracting() {
    return g_interacting;
}

bool WouldCaptureMouse(UIElement* selected, float viewportX, float viewportY) {
    if (!selected || !selected->IsVisible())
        return false;

    UIScreenRect localRect;
    if (!selected->GetScreenRect(localRect))
        return false;

    return HitTestHandle(viewportX, viewportY, localRect, IsResizable(selected), selected->GetPivotScreenPosition()) != GizmoHandle::None;
}

void HandleInput(UIElement* selected) {
    if (!selected || !selected->IsVisible()) {
        g_hoverHandle = GizmoHandle::None;
        if (!Mouse_IsLeftDown())
            g_interacting = false;
        return;
    }

    UIScreenRect localRect;
    if (!selected->GetScreenRect(localRect)) {
        g_hoverHandle = GizmoHandle::None;
        return;
    }

    float localMouseX = 0.0f;
    float localMouseY = 0.0f;
    const bool mouseInViewport = EngineGUI_MouseToViewportLocal(localMouseX, localMouseY);
    const bool resizable = IsResizable(selected);

    if (mouseInViewport && !ImGui::GetIO().WantCaptureMouse)
        g_hoverHandle = HitTestHandle(localMouseX, localMouseY, localRect, resizable, selected->GetPivotScreenPosition());
    else
        g_hoverHandle = GizmoHandle::None;

    const bool lDown = Mouse_IsLeftDown();
    const bool clicked = lDown && !g_prevLDown;
    const bool released = !lDown && g_prevLDown;
    g_prevLDown = lDown;

    if (!mouseInViewport || ImGui::GetIO().WantCaptureMouse) {
        if (released && g_interacting && g_dragElement) {
            const std::string afterJson = SceneCanvas_SerializeElement(*g_dragElement);
            EditorUndo_PushUiEdit(g_dragElement, g_beforeJson, afterJson);
            g_beforeJson.clear();
            g_dragElement = nullptr;
            g_interacting = false;
            g_activeHandle = GizmoHandle::None;
        } else if (released) {
            g_interacting = false;
        }
        return;
    }

    if (clicked && !g_interacting) {
        const GizmoHandle hit = HitTestHandle(localMouseX, localMouseY, localRect, resizable, selected->GetPivotScreenPosition());
        if (hit != GizmoHandle::None) {
            g_interacting = true;
            g_activeHandle = hit;
            g_startRect = localRect;
            g_startPosition = selected->GetPosition();
            GetElementSize(selected, g_startSize.x, g_startSize.y);
            g_dragStartLocalX = localMouseX;
            g_dragStartLocalY = localMouseY;
            g_beforeJson = SceneCanvas_SerializeElement(*selected);
            g_dragElement = selected;
        }
    }

    if (g_interacting && lDown) {
        if (g_activeHandle == GizmoHandle::Move
            || g_activeHandle == GizmoHandle::MoveArrowX
            || g_activeHandle == GizmoHandle::MoveArrowY) {
            ApplyMove(selected, g_activeHandle, localMouseX, localMouseY);
        } else if (resizable) {
            ApplyResize(selected, g_activeHandle, localMouseX, localMouseY);
            EngineGUI_MarkSceneDirty();
        }
    }

    if (released) {
        if (g_interacting && g_dragElement) {
            const std::string afterJson = SceneCanvas_SerializeElement(*g_dragElement);
            EditorUndo_PushUiEdit(g_dragElement, g_beforeJson, afterJson);
            g_beforeJson.clear();
            g_dragElement = nullptr;
        }
        g_interacting = false;
        g_activeHandle = GizmoHandle::None;
    }
}

void Draw(UIElement* selected) {
    if (!selected || !selected->IsVisible())
        return;

    UIScreenRect localRect;
    if (!selected->GetScreenRect(localRect))
        return;

    float winLeft = localRect.left;
    float winTop = localRect.top;
    float winRight = localRect.right;
    float winBottom = localRect.bottom;
    ViewportLocalToWindow(winLeft, winTop);
    ViewportLocalToWindow(winRight, winBottom);

    const DirectX::XMFLOAT2 pivotLocal = selected->GetPivotScreenPosition();
    float pivotWinX = pivotLocal.x;
    float pivotWinY = pivotLocal.y;
    ViewportLocalToWindow(pivotWinX, pivotWinY);

    DirectX::XMFLOAT2 anchorLocal = selected->GetAnchorScreenPosition();
    float anchorWinX = anchorLocal.x;
    float anchorWinY = anchorLocal.y;
    ViewportLocalToWindow(anchorWinX, anchorWinY);

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    const bool resizable = IsResizable(selected);
    const float midX = (winLeft + winRight) * 0.5f;
    const float midY = (winTop + winBottom) * 0.5f;

    drawList->AddRect(
        ImVec2(winLeft, winTop),
        ImVec2(winRight, winBottom),
        kOutlineColor,
        0.0f,
        0,
        2.0f);

    drawList->AddCircleFilled(ImVec2(anchorWinX, anchorWinY), 4.0f, kAnchorColor);
    drawList->AddCircle(ImVec2(anchorWinX, anchorWinY), 4.0f, kOutlineColor);

    DrawMoveArrow(drawList, pivotWinX, pivotWinY, true, g_hoverHandle == GizmoHandle::MoveArrowX);
    DrawMoveArrow(drawList, pivotWinX, pivotWinY, false, g_hoverHandle == GizmoHandle::MoveArrowY);

    if (resizable) {
        DrawHandle(drawList, winLeft, winTop, g_hoverHandle == GizmoHandle::TopLeft);
        DrawHandle(drawList, midX, winTop, g_hoverHandle == GizmoHandle::Top);
        DrawHandle(drawList, winRight, winTop, g_hoverHandle == GizmoHandle::TopRight);
        DrawHandle(drawList, winRight, midY, g_hoverHandle == GizmoHandle::Right);
        DrawHandle(drawList, winRight, winBottom, g_hoverHandle == GizmoHandle::BottomRight);
        DrawHandle(drawList, midX, winBottom, g_hoverHandle == GizmoHandle::Bottom);
        DrawHandle(drawList, winLeft, winBottom, g_hoverHandle == GizmoHandle::BottomLeft);
        DrawHandle(drawList, winLeft, midY, g_hoverHandle == GizmoHandle::Left);
    }
}

} // namespace UiGizmo
