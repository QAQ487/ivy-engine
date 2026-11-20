// inspector_widgets.cpp
// See inspector_widgets.h. Labels are drawn through the draw list behind a
// fixed-size Dummy so long words never widen the window content.

#include "engine/editor/inspector_widgets.h"

#include "engine/ecs/component.h"
#include "engine/editor/editor_icon_button.h"
#include "engine/editor/inspector_layout_math.h"
#include "imgui/icons_font_awesome.h"
#include "imgui/imgui_internal.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <cstring>

namespace insp {
namespace {

ImVec4 g_currentAccent = ImVec4(0.6f, 0.65f, 0.7f, 1.0f);

bool LightBackground()
{
    const ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    return inspector_layout::IsLightBackground(bg.x, bg.y, bg.z);
}

ImVec4 AdaptToBackground(ImVec4 c)
{
    if (LightBackground()) {
        c.x *= 0.68f;
        c.y *= 0.68f;
        c.z *= 0.68f;
    }
    return c;
}

ImU32 U32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

// Fixed slot for a row/header icon so the words after it line up.
float IconSlot() { return ImGui::GetFontSize() * 1.35f; }

// Draw a single icon glyph centered by its ink box (not its advance) inside
// [x0, x0 + w] x [y0, y0 + h]. Font Awesome glyphs have uneven side bearings,
// so advance-based centering leaves them visibly off-center.
void DrawIconCentered(ImDrawList* dl, float x0, float y0, float w, float h, ImU32 col, const char* icon)
{
    if (!icon || !icon[0])
        return;
    const char* end = icon + std::strlen(icon);
    unsigned int cp = 0;
    const int bytes = ImTextCharFromUtf8(&cp, icon, end);
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize();
    ImFontBaked* baked = font ? font->GetFontBaked(size) : nullptr;
    const ImFontGlyph* g = (baked && bytes > 0 && cp) ? baked->FindGlyph((ImWchar)cp) : nullptr;
    if (g && g->Visible) {
        const float scale = baked->Size > 0.0f ? size / baked->Size : 1.0f;
        float x = 0.0f, y = 0.0f;
        editor_icon_button::CenteredGlyphDrawPos(x0, y0, x0 + w, y0 + h,
            g->X0, g->Y0, g->X1, g->Y1, scale, &x, &y);
        dl->AddText(font, size, ImVec2(x, y), col, icon, icon + bytes);
    } else {
        const ImVec2 ts = ImGui::CalcTextSize(icon);
        dl->AddText(ImVec2(x0 + (w - ts.x) * 0.5f, y0 + (h - ts.y) * 0.5f), col, icon);
    }
}

bool WordFits(const char* icon, const char* word, float w)
{
    const ImGuiStyle& st = ImGui::GetStyle();
    return word && word[0] &&
        ImGui::CalcTextSize(icon).x + ImGui::CalcTextSize(word).x + ImGui::GetFontSize() * 0.4f +
            st.FramePadding.x * 2.0f <= w;
}

// Words are shown for a whole row of buttons or for none of them.
bool AllWordsFit(const char* const* icons, const char* const* words, int count, float w)
{
    if (!words)
        return false;
    for (int i = 0; i < count; ++i)
        if (!WordFits(icons[i], words[i], w))
            return false;
    return true;
}

void Tooltip(const char* word, const char* tip)
{
    if (word && tip && tip[0] && std::strcmp(word, tip) != 0)
        ImGui::SetTooltip("%s\n%s", word, tip);
    else if (word && word[0])
        ImGui::SetTooltip("%s", word);
    else if (tip && tip[0])
        ImGui::SetTooltip("%s", tip);
}

bool HasExtension(const char* path, const char* const* exts, int count)
{
    if (!path)
        return false;
    const char* dot = std::strrchr(path, '.');
    if (!dot)
        return false;
    std::string ext(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (int i = 0; i < count; ++i)
        if (ext == exts[i])
            return true;
    return false;
}

// Button that shows "icon word" when it fits, else the icon only.
bool LabelledButton(const char* id, const char* icon, const char* word, float w, bool active,
                    bool showWord, const char* warn = nullptr)
{
    if (active) {
        ImVec4 a = g_currentAccent;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(a.x, a.y, a.z, 0.55f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(a.x, a.y, a.z, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(a.x, a.y, a.z, 0.85f));
    }
    if (warn)
        ImGui::PushStyleColor(ImGuiCol_Text, WarningColor());
    char label[160];
    bool pressed = false;
    if (showWord) {
        std::snprintf(label, sizeof(label), "%s %s##%s", icon, word, id);
        pressed = ImGui::Button(label, ImVec2(w, ImGui::GetFrameHeight()));
    } else {
        std::snprintf(label, sizeof(label), "%s##%s", icon, id);
        // Active state is carried by the pushed Button color, so draw as a plain button.
        pressed = editor_icon_button::IconButton(label, ImVec2(w, ImGui::GetFrameHeight()));
    }
    if (warn)
        ImGui::PopStyleColor();
    if (active)
        ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        Tooltip(word, warn);
    return pressed;
}

} // namespace

ImVec4 AccentColor(Accent accent)
{
    ImVec4 c;
    switch (accent) {
    case Accent::Transform: c = ImVec4(0.55f, 0.66f, 0.80f, 1.0f); break;
    case Accent::Render:    c = ImVec4(0.38f, 0.80f, 0.55f, 1.0f); break;
    case Accent::Light:     c = ImVec4(0.98f, 0.78f, 0.30f, 1.0f); break;
    case Accent::Camera:    c = ImVec4(0.70f, 0.55f, 0.95f, 1.0f); break;
    case Accent::Physics:   c = ImVec4(0.96f, 0.55f, 0.30f, 1.0f); break;
    case Accent::Audio:     c = ImVec4(0.95f, 0.50f, 0.70f, 1.0f); break;
    case Accent::Script:    c = ImVec4(0.35f, 0.78f, 0.98f, 1.0f); break;
    case Accent::Effect:    c = ImVec4(0.90f, 0.45f, 0.95f, 1.0f); break;
    default:                c = ImVec4(0.62f, 0.66f, 0.70f, 1.0f); break;
    }
    return AdaptToBackground(c);
}

void SetAccent(Accent accent) { g_currentAccent = AccentColor(accent); }

bool PivotRow(float* xy, const void* owner)
{
    static const char* names[] = { "Center", "Top Left", "Top", "Top Right", "Left", "Right",
        "Bottom Left", "Bottom", "Bottom Right", "Custom" };
    static const float values[9][2] = { {.5f, .5f}, {0, 1}, {.5f, 1}, {1, 1},
        {0, .5f}, {1, .5f}, {0, 0}, {.5f, 0}, {1, 0} };
    static const void* lastOwner = nullptr;
    static bool custom = false;
    if (lastOwner != owner) { lastOwner = owner; custom = false; }
    int current = 9;
    for (int i = 0; i < 9; ++i) {
        const float dx = xy[0] - values[i][0], dy = xy[1] - values[i][1];
        if (dx * dx < 1e-10f && dy * dy < 1e-10f) { current = i; break; }
    }
    if (custom) current = 9;
    bool changed = false;
    {
        PropRow r(ICON_FA_CROSSHAIRS, "Pivot");
        if (ImGui::Combo("##v", &current, names, IM_ARRAYSIZE(names))) {
            custom = current == 9;
            if (!custom) { xy[0] = values[current][0]; xy[1] = values[current][1]; changed = true; }
        }
    }
    if (current == 9)
        changed |= DragVecN(nullptr, "Custom", "Pivot (0-1, bottom-left)", xy, 2, 0.01f, 0.0f, 1.0f, 0.5f);
    return changed;
}

ImVec4 WarningColor() { return AdaptToBackground(ImVec4(0.98f, 0.72f, 0.22f, 1.0f)); }
ImVec4 InfoColor()    { return AdaptToBackground(ImVec4(0.45f, 0.70f, 1.00f, 1.0f)); }
ImVec4 ErrorColor()   { return AdaptToBackground(ImVec4(1.00f, 0.45f, 0.35f, 1.0f)); }

HeaderResult ComponentHeader(const char* id, const char* icon, const char* name, Accent accent,
                             bool* enabled, const Component* component,
                             bool canRemove, bool canReset,
                             void (*extraMenu)(void*), void* extraMenuUser)
{
    HeaderResult result;
    const ImGuiStyle& st = ImGui::GetStyle();
    const float font = ImGui::GetFontSize();
    const float frameH = ImGui::GetFrameHeight();
    const ImVec4 acc = AccentColor(accent);
    g_currentAccent = acc;

    ImGui::PushID(id);
    ImGui::Spacing();
    // Framed headers reach half the window padding past the content edge (as
    // CollapsingHeader does); keep the right-side buttons inside the content.
    const float contentMaxX = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen |
        ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_NoTreePushOnOpen |
        ImGuiTreeNodeFlags_SpanAvailWidth;
    result.open = ImGui::TreeNodeEx("##hdr", flags);
    const bool rightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
    const ImVec2 mn = ImGui::GetItemRectMin();
    const ImVec2 mx = ImGui::GetItemRectMax();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(mn, ImVec2(mn.x + (std::max)(2.0f, font * 0.2f), mx.y), U32(acc));

    const int buttons = (enabled ? 1 : 0) + 1;
    const float gap = st.ItemInnerSpacing.x;
    const float rightW = buttons * frameH + (buttons - 1) * gap;
    const float textX = mn.x + font + st.FramePadding.x * 2.0f;
    const float rightX = (std::min)(mx.x, contentMaxX);
    const float textMaxX = (std::max)(textX, rightX - rightW - gap);
    const bool isOn = !enabled || *enabled;
    const ImVec4 textCol = ImGui::GetStyleColorVec4(isOn ? ImGuiCol_Text : ImGuiCol_TextDisabled);
    ImVec4 iconCol = acc;
    if (!isOn)
        iconCol.w = 0.45f;
    dl->PushClipRect(ImVec2(textX, mn.y), ImVec2(textMaxX, mx.y), true);
    const float textY = mn.y + (mx.y - mn.y - font) * 0.5f;
    if (icon)
        DrawIconCentered(dl, textX, mn.y, font * 1.1f, mx.y - mn.y, U32(iconCol), icon);
    if (name)
        dl->AddText(ImVec2(textX + (icon ? IconSlot() : 0.0f), textY), U32(textCol), name);
    dl->PopClipRect();

    // Right-side controls overlap the header frame.
    float bx = rightX - rightW;
    if (enabled) {
        ImGui::SetCursorScreenPos(ImVec2(bx, mn.y));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, *enabled ? acc : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        if (editor_icon_button::IconButton(*enabled ? ICON_FA_TOGGLE_ON "##en" : ICON_FA_TOGGLE_OFF "##en", ImVec2(frameH, frameH))) {
            *enabled = !*enabled;
            result.changed = true;
            ImGui::MarkItemEdited(ImGui::GetItemID());
        }
        ImGui::PopStyleColor(2);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip(*enabled ? "On" : "Off");
        bx += frameH + gap;
    }
    ImGui::SetCursorScreenPos(ImVec2(bx, mn.y));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    const bool menuClicked = editor_icon_button::IconButton(ICON_FA_ELLIPSIS_VERTICAL "##menu", ImVec2(frameH, frameH));
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Menu");
    if (menuClicked || rightClicked)
        ImGui::OpenPopup("##hdrmenu");
    if (ImGui::BeginPopup("##hdrmenu")) {
        if (name)
            ImGui::TextDisabled("%s", name);
        ImGui::Separator();
        if (extraMenu) {
            extraMenu(extraMenuUser);
            ImGui::Separator();
        }
        if (ImGui::MenuItem(ICON_FA_ROTATE_LEFT "  Reset", nullptr, false, canReset))
            result.action = HeaderAction::Reset;
        if (ImGui::MenuItem(ICON_FA_COPY "  Copy JSON", nullptr, false, component != nullptr))
            ImGui::SetClipboardText(component->Serialize().c_str());
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_TRASH_CAN "  Remove", nullptr, false, canRemove))
            result.action = HeaderAction::Remove;
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return result;
}

float RowLabel(const char* icon, const char* word, const char* tip)
{
    const ImGuiStyle& st = ImGui::GetStyle();
    const float avail = ImGui::GetContentRegionAvail().x;
    if (!icon && !word)
        return (std::max)(1.0f, avail);

    const inspector_layout::RowLayout L = inspector_layout::ComputeRowLayout(
        avail, ImGui::GetFontSize(), st.ItemSpacing.x, icon != nullptr);
    const float frameH = ImGui::GetFrameHeight();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(L.labelW, frameH));
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float y = p.y + st.FramePadding.y;
    dl->PushClipRect(p, ImVec2(p.x + L.labelW, p.y + frameH), true);
    float x = p.x;
    if (icon) {
        DrawIconCentered(dl, x, p.y, ImGui::GetFontSize() * 1.1f, frameH, U32(g_currentAccent), icon);
        x += IconSlot();
    }
    bool clipped = !L.showText;
    if (word && L.showText) {
        dl->AddText(ImVec2(x, y), ImGui::GetColorU32(ImGuiCol_Text), word);
        clipped = x + ImGui::CalcTextSize(word).x > p.x + L.labelW;
    }
    dl->PopClipRect();
    if (hovered && (clipped || (tip && tip[0])))
        Tooltip(word, tip);

    ImGui::SameLine(0.0f, st.ItemSpacing.x);
    return L.fieldW;
}

PropRow::PropRow(const char* icon, const char* word, const char* tip)
{
    ImGui::PushID(word ? word : (icon ? icon : "row"));
    RowLabel(icon, word, tip);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

PropRow::~PropRow()
{
    ImGui::PopID();
}

bool DragVecN(const char* icon, const char* word, const char* tip, float* v, int n,
              float speed, float vmin, float vmax, float resetValue, float wheelStep,
              const char* fmt, const char* axes)
{
    auto axisColor = [](char a) {
        switch (a) {
        case 'X': return ImVec4(0.86f, 0.30f, 0.30f, 1.0f);
        case 'Y': return ImVec4(0.40f, 0.74f, 0.30f, 1.0f);
        case 'Z': return ImVec4(0.30f, 0.52f, 0.92f, 1.0f);
        default:  return ImVec4(0.55f, 0.55f, 0.58f, 1.0f);
        }
    };
    if (!axes || std::strlen(axes) < static_cast<size_t>(std::clamp(n, 1, 4)))
        axes = "XYZW";
    n = std::clamp(n, 1, 4);

    const ImGuiStyle& st = ImGui::GetStyle();
    const float font = ImGui::GetFontSize();
    const float frameH = ImGui::GetFrameHeight();
    const float sp = st.ItemInnerSpacing.x;

    ImGui::PushID(word ? word : (icon ? icon : "vec"));
    const float fieldW = RowLabel(icon, word, tip);
    const float startX = ImGui::GetCursorPosX();
    const bool stack = inspector_layout::StackVecAxes(fieldW, n, font, sp);
    const float cellW = stack ? fieldW : inspector_layout::SplitWidth(fieldW, n, sp);
    const float chipW = (std::min)(font * 0.95f, cellW * 0.4f);

    bool changed = false;
    for (int i = 0; i < n; ++i) {
        ImGui::PushID(i);
        if (i > 0) {
            if (stack)
                ImGui::SetCursorPosX(startX);
            else
                ImGui::SameLine(0.0f, sp);
        }
        const ImVec2 cp = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##chip", ImVec2(chipW, frameH))) {
            if (v[i] != resetValue) {
                v[i] = resetValue;
                changed = true;
                ImGui::MarkItemEdited(ImGui::GetItemID());
            }
        }
        const char axisName[2] = { axes[i], 0 };
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s = %g", axisName, resetValue);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec4 c = axisColor(axes[i]);
        if (ImGui::IsItemHovered())
            c = ImVec4((std::min)(1.0f, c.x * 1.2f), (std::min)(1.0f, c.y * 1.2f), (std::min)(1.0f, c.z * 1.2f), 1.0f);
        dl->AddRectFilled(cp, ImVec2(cp.x + chipW, cp.y + frameH), U32(c), st.FrameRounding,
            ImDrawFlags_RoundCornersLeft);
        const ImVec2 ts = ImGui::CalcTextSize(axisName);
        dl->AddText(ImVec2(cp.x + (chipW - ts.x) * 0.5f, cp.y + (frameH - ts.y) * 0.5f),
            IM_COL32(255, 255, 255, 235), axisName);

        ImGui::SameLine(0.0f, 0.0f);
        ImGui::SetNextItemWidth((std::max)(1.0f, cellW - chipW));
        if (ImGui::DragFloat("##a", &v[i], speed, vmin, vmax, fmt))
            changed = true;
        if (wheelStep > 0.0f && ImGui::IsItemHovered()) {
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) {
                v[i] += wheel * wheelStep;
                if (vmax > vmin)
                    v[i] = std::clamp(v[i], vmin, vmax);
                changed = true;
            }
        }
        ImGui::PopID();
    }
    ImGui::PopID();
    return changed;
}

bool ToggleRow(const Toggle* toggles, int count)
{
    if (!toggles || count <= 0)
        return false;
    const ImGuiStyle& st = ImGui::GetStyle();
    const float avail = ImGui::GetContentRegionAvail().x;
    const int cols = inspector_layout::WrapColumns(avail, ImGui::GetFrameHeight() * 1.6f,
        st.ItemSpacing.x, count);
    const float w = inspector_layout::SplitWidth(avail, cols, st.ItemSpacing.x);
    bool words = true;
    for (int i = 0; i < count; ++i)
        words = words && WordFits(toggles[i].icon, toggles[i].tip, w);
    bool changed = false;
    for (int i = 0; i < count; ++i) {
        if (i % cols != 0)
            ImGui::SameLine();
        char id[16];
        std::snprintf(id, sizeof(id), "t%d", i);
        bool* value = toggles[i].value;
        if (LabelledButton(id, toggles[i].icon, toggles[i].tip, w, value && *value, words, toggles[i].warn) &&
            value) {
            *value = !*value;
            changed = true;
            ImGui::MarkItemEdited(ImGui::GetItemID());
        }
    }
    return changed;
}

bool ChoiceBar(const char* id, const char* const* icons, const char* const* tips, int count,
               int* selected)
{
    if (!icons || !selected || count <= 0)
        return false;
    const ImGuiStyle& st = ImGui::GetStyle();
    const float avail = ImGui::GetContentRegionAvail().x;
    const int cols = inspector_layout::WrapColumns(avail, ImGui::GetFrameHeight() * 1.4f,
        st.ItemInnerSpacing.x, count);
    const float w = inspector_layout::SplitWidth(avail, cols, st.ItemInnerSpacing.x);
    const bool words = AllWordsFit(icons, tips, count, w);
    bool changed = false;
    ImGui::PushID(id);
    for (int i = 0; i < count; ++i) {
        if (i % cols != 0)
            ImGui::SameLine(0.0f, st.ItemInnerSpacing.x);
        char bid[16];
        std::snprintf(bid, sizeof(bid), "c%d", i);
        const bool active = (*selected == i);
        if (LabelledButton(bid, icons[i], tips ? tips[i] : nullptr, w, active, words) && !active) {
            *selected = i;
            changed = true;
            ImGui::MarkItemEdited(ImGui::GetItemID());
        }
    }
    ImGui::PopID();
    return changed;
}

int IconButtonRow(const char* id, const char* const* icons, const char* const* tips, int count)
{
    if (!icons || count <= 0)
        return -1;
    const ImGuiStyle& st = ImGui::GetStyle();
    const float avail = ImGui::GetContentRegionAvail().x;
    const int cols = inspector_layout::WrapColumns(avail, ImGui::GetFrameHeight() * 1.4f,
        st.ItemSpacing.x, count);
    const float w = inspector_layout::SplitWidth(avail, cols, st.ItemSpacing.x);
    const bool words = AllWordsFit(icons, tips, count, w);
    int pressed = -1;
    ImGui::PushID(id);
    for (int i = 0; i < count; ++i) {
        if (i % cols != 0)
            ImGui::SameLine();
        char bid[16];
        std::snprintf(bid, sizeof(bid), "b%d", i);
        if (LabelledButton(bid, icons[i], tips ? tips[i] : nullptr, w, false, words))
            pressed = i;
    }
    ImGui::PopID();
    return pressed;
}

bool AssetField(const char* icon, const char* word, const char* tip, std::string& path,
                const char* const* exts, int extCount, bool commitOnEnter)
{
    const ImGuiStyle& st = ImGui::GetStyle();
    const float frameH = ImGui::GetFrameHeight();
    ImGui::PushID(word ? word : (icon ? icon : "asset"));
    const float fieldW = RowLabel(icon, word, tip);
    const bool has = !path.empty();
    const float clearW = has ? frameH + st.ItemInnerSpacing.x : 0.0f;

    bool changed = false;
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s", path.c_str());
    ImGui::SetNextItemWidth((std::max)(1.0f, fieldW - clearW));
    if (ImGui::InputTextWithHint("##p", "None", buf, sizeof(buf),
            commitOnEnter ? ImGuiInputTextFlags_EnterReturnsTrue : 0)) {
        path = buf;
        changed = true;
    }
    if (has && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", path.c_str());
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            const char* dropped = static_cast<const char*>(payload->Data);
            if (dropped && (extCount <= 0 || HasExtension(dropped, exts, extCount)) && path != dropped) {
                path = dropped;
                changed = true;
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (has) {
        ImGui::SameLine(0.0f, st.ItemInnerSpacing.x);
        if (editor_icon_button::IconButton(ICON_FA_XMARK "##clr", ImVec2(frameH, frameH))) {
            path.clear();
            changed = true;
            ImGui::MarkItemEdited(ImGui::GetItemID());
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("Clear");
    }
    ImGui::PopID();
    return changed;
}

bool IconButton(const char* iconId, const char* tip, float size)
{
    const float s = size > 0.0f ? size : ImGui::GetFrameHeight();
    const bool pressed = editor_icon_button::IconButton(iconId, ImVec2(s, s));
    if (tip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", tip);
    return pressed;
}

void HintIcon(const char* icon, const ImVec4& color, const char* tip)
{
    ImGui::TextColored(color, "%s", icon);
    if (tip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", tip);
}

bool WideButton(const char* icon, const char* word, const char* tip)
{
    char label[160];
    std::snprintf(label, sizeof(label), "%s  %s", icon ? icon : "", word ? word : "");
    const bool pressed = ImGui::Button(label, ImVec2(-FLT_MIN, 0.0f));
    if (tip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", tip);
    return pressed;
}

void SubHeading(const char* icon, const char* word, const char* hint, bool hintIsWarning)
{
    // Like SeparatorText, but clipped to the available width.
    const float avail = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
    const float h = ImGui::GetTextLineHeightWithSpacing();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(avail, h));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 muted = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    const float y = p.y + (h - ImGui::GetFontSize()) * 0.5f;
    dl->PushClipRect(p, ImVec2(p.x + avail, p.y + h), true);
    float x = p.x;
    if (icon) {
        DrawIconCentered(dl, x, p.y, ImGui::GetFontSize() * 1.1f, h, U32(g_currentAccent), icon);
        x += IconSlot();
    }
    if (word) {
        dl->AddText(ImVec2(x, y), muted, word);
        x += ImGui::CalcTextSize(word).x;
    }
    x += ImGui::GetFontSize() * 0.5f;
    float lineEnd = p.x + avail;
    if (hint) {
        const char* hintIcon = hintIsWarning ? ICON_FA_TRIANGLE_EXCLAMATION : ICON_FA_CIRCLE_INFO;
        const float iw = ImGui::CalcTextSize(hintIcon).x;
        const ImVec2 hp(p.x + avail - iw, y);
        DrawIconCentered(dl, hp.x, p.y, iw, h, U32(hintIsWarning ? WarningColor() : InfoColor()), hintIcon);
        lineEnd = hp.x - ImGui::GetFontSize() * 0.4f;
        if (ImGui::IsMouseHoveringRect(ImVec2(hp.x, p.y), ImVec2(p.x + avail, p.y + h)) &&
            ImGui::IsWindowHovered())
            ImGui::SetTooltip("%s", hint);
    }
    if (x < lineEnd) {
        const float ly = static_cast<float>(static_cast<int>(p.y + h * 0.5f));
        dl->AddLine(ImVec2(x, ly), ImVec2(lineEnd, ly), ImGui::GetColorU32(ImGuiCol_Separator));
    }
    dl->PopClipRect();
}

void ReadOnlyRow(const char* icon, const char* word, const char* tip, const char* text,
                 const ImVec4* color)
{
    ImGui::PushID(word ? word : (icon ? icon : "ro"));
    const float fieldW = RowLabel(icon, word, tip);
    const float frameH = ImGui::GetFrameHeight();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(fieldW, frameH));
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(p, ImVec2(p.x + fieldW, p.y + frameH), true);
    dl->AddText(ImVec2(p.x, p.y + ImGui::GetStyle().FramePadding.y),
        color ? U32(*color) : ImGui::GetColorU32(ImGuiCol_TextDisabled), text ? text : "");
    dl->PopClipRect();
    if (hovered && text && text[0])
        Tooltip(text, tip);
    ImGui::PopID();
}

std::string Ellipsize(const std::string& text, float maxW)
{
    if (ImGui::CalcTextSize(text.c_str()).x <= maxW)
        return text;
    const float dotsW = ImGui::CalcTextSize("...").x;
    std::string out = text;
    while (!out.empty() && ImGui::CalcTextSize(out.c_str()).x + dotsW > maxW) {
        out.pop_back();
        // Do not cut inside a UTF-8 sequence.
        while (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0) == 0x80)
            out.pop_back();
        if (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0) == 0xC0)
            out.pop_back();
    }
    return out + "...";
}

} // namespace insp
