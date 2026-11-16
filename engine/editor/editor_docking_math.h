// editor_docking_math.h
// Pure helpers for docking default splits, ini path resolution, and client→backbuffer
// viewport conversion (Linux-testable; no ImGui / D3D dependency).
#pragma once

#include "engine/core/utf8_path.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

struct EditorDefaultPanelSizes {
    float leftWidth = 0.0f;
    float rightWidth = 0.0f;
    float bottomHeight = 0.0f;
};

struct EditorDockSplitRatios {
    // DockBuilderSplitNode size_ratio_for_node_at_dir values, applied in order:
    // Left off root, then Right off remaining, then Down off remaining.
    float left = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

struct EditorClientRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct EditorBackbufferRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

inline constexpr float kEditorDockLeftPanelWidth = 300.0f;
inline constexpr float kEditorDockRightPanelWidth = 350.0f;
inline constexpr float kEditorDockBottomPanelMinHeight = 340.0f;
inline constexpr float kEditorDockBottomPanelScreenFraction = 0.34f;
inline constexpr float kEditorDockViewportMinHeight = 120.0f;
inline constexpr float kEditorDockViewportMinWidth = 64.0f;
// Editor / F2 / F3 game view is locked to 16:9 (letterbox or pillarbox inside bounds).
inline constexpr float kEditorViewportAspectW = 16.0f;
inline constexpr float kEditorViewportAspectH = 9.0f;

// Largest centered rect of aspect (aspectW:aspectH) that fits inside bounds.
// Degenerate / non-finite bounds clamp to a 1x1 rect at the origin of bounds.
inline EditorClientRect FitFixedAspectRect(
    const EditorClientRect& bounds, float aspectW, float aspectH)
{
    EditorClientRect out{};
    float bx = std::isfinite(bounds.x) ? bounds.x : 0.0f;
    float by = std::isfinite(bounds.y) ? bounds.y : 0.0f;
    float bw = (std::isfinite(bounds.w) && bounds.w > 0.0f) ? bounds.w : 0.0f;
    float bh = (std::isfinite(bounds.h) && bounds.h > 0.0f) ? bounds.h : 0.0f;
    if (bw < 1.0f) bw = 1.0f;
    if (bh < 1.0f) bh = 1.0f;

    const float aw = (aspectW > 0.0f && std::isfinite(aspectW)) ? aspectW : 16.0f;
    const float ah = (aspectH > 0.0f && std::isfinite(aspectH)) ? aspectH : 9.0f;
    const float targetAspect = aw / ah;
    const float boundsAspect = bw / bh;

    float w = 0.0f;
    float h = 0.0f;
    if (boundsAspect > targetAspect) {
        // Wider than target → pillarbox (left/right bars).
        h = bh;
        w = h * targetAspect;
    } else {
        // Taller or equal → letterbox (top/bottom bars).
        w = bw;
        h = w / targetAspect;
    }

    if (w > bw) w = bw;
    if (h > bh) h = bh;

    // Prefer aspect; only inflate below 1px when bounds allow.
    if (w < 1.0f && bw >= 1.0f) {
        w = 1.0f;
        h = w / targetAspect;
        if (h > bh) h = bh;
    }
    if (h < 1.0f && bh >= 1.0f) {
        h = 1.0f;
        w = h * targetAspect;
        if (w > bw) w = bw;
    }
    if (w < 1.0f) w = 1.0f;
    if (h < 1.0f) h = 1.0f;
    if (w > bw) w = bw;
    if (h > bh) h = bh;

    out.x = bx + (bw - w) * 0.5f;
    out.y = by + (bh - h) * 0.5f;
    out.w = w;
    out.h = h;

    // Keep edges inside bounds after float rounding.
    if (out.x < bx) out.x = bx;
    if (out.y < by) out.y = by;
    if (out.x + out.w > bx + bw) out.w = (bx + bw) - out.x;
    if (out.y + out.h > by + bh) out.h = (by + bh) - out.y;
    if (out.w < 1.0f) out.w = 1.0f;
    if (out.h < 1.0f) out.h = 1.0f;
    return out;
}

inline EditorClientRect FitEditor16x9Viewport(const EditorClientRect& bounds)
{
    return FitFixedAspectRect(bounds, kEditorViewportAspectW, kEditorViewportAspectH);
}

inline EditorDefaultPanelSizes ComputeDefaultPanelSizes(
    float hostW, float hostH, float uiZoom)
{
    const float z = (uiZoom > 0.0f) ? uiZoom : 1.0f;
    EditorDefaultPanelSizes s;
    s.leftWidth = kEditorDockLeftPanelWidth * z;
    s.rightWidth = kEditorDockRightPanelWidth * z;
    s.bottomHeight = (std::max)(
        kEditorDockBottomPanelMinHeight * z,
        hostH * kEditorDockBottomPanelScreenFraction);
    const float maxBottom = hostH - kEditorDockViewportMinHeight * z;
    if (maxBottom > 0.0f)
        s.bottomHeight = (std::min)(s.bottomHeight, maxBottom);
    if (s.bottomHeight < 1.0f)
        s.bottomHeight = 1.0f;
    // Keep side panels from consuming the whole host.
    const float maxSide = (std::max)(1.0f, hostW * 0.45f);
    s.leftWidth = (std::min)(s.leftWidth, maxSide);
    s.rightWidth = (std::min)(s.rightWidth, maxSide);
    return s;
}

inline EditorDockSplitRatios ComputeDefaultDockSplitRatios(
    float hostW, float hostH, float uiZoom)
{
    const EditorDefaultPanelSizes sizes = ComputeDefaultPanelSizes(hostW, hostH, uiZoom);
    EditorDockSplitRatios r;
    const float w = (std::max)(1.0f, hostW);
    const float h = (std::max)(1.0f, hostH);
    r.left = (std::min)(0.45f, sizes.leftWidth / w);
    const float afterLeft = (std::max)(1.0f, w * (1.0f - r.left));
    r.right = (std::min)(0.45f, sizes.rightWidth / afterLeft);
    r.bottom = (std::min)(0.60f, sizes.bottomHeight / h);
    if (r.left < 0.05f) r.left = 0.05f;
    if (r.right < 0.05f) r.right = 0.05f;
    if (r.bottom < 0.10f) r.bottom = 0.10f;
    return r;
}

// Relative path under a content project root (UTF-8, generic separators).
inline std::string ResolveEditorImGuiLayoutIniPath(std::string_view projectRootUtf8)
{
    if (projectRootUtf8.empty())
        return {};
    const auto root = PathFromUtf8(projectRootUtf8);
    return PathToUtf8(root / "rom" / "editor" / "imgui_layout.ini");
}

// Convert an ImGui-client axis-aligned rect to backbuffer pixels.
// scaleX = backbufferW / clientW (and same for Y). Safe for zero client size.
inline EditorBackbufferRect ClientRectToBackbuffer(
    const EditorClientRect& client,
    float clientW, float clientH,
    float backbufferW, float backbufferH)
{
    EditorBackbufferRect out{};
    const float sx = (clientW > 0.0f) ? (backbufferW / clientW) : 1.0f;
    const float sy = (clientH > 0.0f) ? (backbufferH / clientH) : 1.0f;
    const float x0 = client.x * sx;
    const float y0 = client.y * sy;
    const float x1 = (client.x + client.w) * sx;
    const float y1 = (client.y + client.h) * sy;
    out.x = x0;
    out.y = y0;
    out.w = x1 - x0;
    out.h = y1 - y0;
    if (!std::isfinite(out.w) || out.w < 1.0f) out.w = 1.0f;
    if (!std::isfinite(out.h) || out.h < 1.0f) out.h = 1.0f;
    if (!std::isfinite(out.x)) out.x = 0.0f;
    if (!std::isfinite(out.y)) out.y = 0.0f;
    return out;
}

inline bool IsUsableViewportRect(const EditorClientRect& r)
{
    return r.w >= kEditorDockViewportMinWidth && r.h >= kEditorDockViewportMinHeight
        && std::isfinite(r.x) && std::isfinite(r.y)
        && std::isfinite(r.w) && std::isfinite(r.h);
}
