// inspector_layout_math.h
// Pure layout rules for the Properties (inspector) panel. Every width the panel
// uses is derived from the available content width so nothing ever needs a
// horizontal scrollbar. Linux-tested by tests/inspector_layout_math_test.cpp.
#pragma once

#include <algorithm>

namespace inspector_layout {

// Widths are in pixels; `font` is the current font size (it already carries the
// editor UI scale, so every threshold scales with UI zoom).
struct RowLayout {
    float labelW = 0.0f;   // label column width (0 when there is no label)
    float fieldW = 0.0f;   // widget width after the label column + spacing
    bool showText = true;  // false = icon-only label (narrow panel)
};

// Panel narrower than this shows icon-only labels.
inline float IconOnlyThreshold(float font) { return font * 14.0f; }

inline RowLayout ComputeRowLayout(float avail, float font, float spacing, bool hasIcon)
{
    RowLayout r;
    avail = (std::max)(avail, 1.0f);
    r.showText = !hasIcon || avail >= IconOnlyThreshold(font);
    if (r.showText)
        r.labelW = std::clamp(avail * 0.35f, font * 2.2f, font * 7.0f);
    else
        r.labelW = font * 1.6f;
    // Never let the label take more than half of a tiny panel.
    r.labelW = (std::min)(r.labelW, avail * 0.5f);
    r.fieldW = (std::max)(1.0f, avail - r.labelW - spacing);
    return r;
}

// Width of each of `n` equal cells filling `avail` with `spacing` between them.
inline float SplitWidth(float avail, int n, float spacing)
{
    if (n <= 0)
        return 0.0f;
    const float w = (avail - spacing * static_cast<float>(n - 1)) / static_cast<float>(n);
    return (std::max)(1.0f, w);
}

// A Vec3 row stacks one axis per line once a single axis field would be
// narrower than this.
inline bool StackVecAxes(float fieldW, int axes, float font, float spacing)
{
    return SplitWidth(fieldW, axes, spacing) < font * 3.5f;
}

// Columns for `count` equal buttons of at least `minItemW` each.
inline int WrapColumns(float avail, float minItemW, float spacing, int count)
{
    if (count <= 0)
        return 1;
    const float step = (std::max)(1.0f, minItemW + spacing);
    int cols = static_cast<int>((avail + spacing) / step);
    return std::clamp(cols, 1, count);
}

// True when the colors behind the panel are light (darken accents there).
inline bool IsLightBackground(float r, float g, float b)
{
    return 0.2126f * r + 0.7152f * g + 0.0722f * b > 0.5f;
}

} // namespace inspector_layout
