// editor_ui_panel.cpp
// Editor panel for inspecting/editing UI canvases.

#include "editor_ui_panel.h"
#include "engine/core/engine_gui.h"
#include "engine/core/utf8_path.h"
#include "engine/editor/editor_undo.h"
#include "engine/editor/inspector_widgets.h"

#include "imgui/imgui.h"
#include "imgui/icons_font_awesome.h"
#include "../ui/scene_canvas.h"
#include "../ui/ui_bar.h"
#include "../ui/ui_button.h"
#include "../ui/ui_canvas.h"
#include "../ui/ui_font.h"
#include "../ui/ui_sprite.h"
#include "../ui/ui_text.h"
#include "../scripting/ui_canvas_script_factory.h"
#include "engine/ecs/game_object.h"

#include <algorithm>
#include <cfloat>
#include <cctype>
#include <filesystem>
#include <string>

namespace {

namespace fs = std::filesystem;

static const char* kTextureExts[] = { ".png", ".jpg", ".jpeg", ".dds", ".tga" };
static const char* kFontExts[] = { ".ttf", ".otf", ".png" };

std::string LowerAssetExtension(const char* path) {
    if (!path) return "";
    std::string ext = PathToUtf8(PathFromUtf8(path).extension());
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

// Path field on a char buffer: Enter commits typed text; drop / clear apply at once.
bool PathRow(const char* icon, const char* word, const char* tip, char* buf, size_t size,
             const char* const* exts, int extCount) {
    std::string path(buf);
    if (!insp::AssetField(icon, word, tip, path, exts, extCount, true))
        return false;
    strncpy_s(buf, size, path.c_str(), _TRUNCATE);
    return true;
}

// Font path (+ built-in font button) and, for PNG atlases, char W / H / per row.
bool DrawFontRows(char* buf, size_t size, float& charW, float& charH, int& perRow,
                  const char* fontTip = nullptr) {
    insp::SubHeading(ICON_FA_FONT, "Font", fontTip, false);
    bool changed = PathRow(ICON_FA_FONT, "Font", "TTF / OTF, or PNG atlas", buf, size,
        kFontExts, IM_ARRAYSIZE(kFontExts));
    // The engine's built-in font lives in the exe, not in the project's rom/, so the
    // Asset Browser cannot offer it. This button fills in its "builtin:" path.
    static const char* const kIcons[] = { ICON_FA_FONT };
    static const char* const kTips[] = { "Built-in" };
    if (insp::IconButtonRow("builtinfont", kIcons, kTips, 1) == 0) {
        strncpy_s(buf, size, kUIFontDefaultPath, _TRUNCATE);
        changed = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s\nBuilt into the engine, not a rom/ file", kUIFontDefaultPath);
    if (LowerAssetExtension(buf) == ".png") {
        float cell[2] = { charW, charH };
        if (insp::DragVecN(ICON_FA_BORDER_ALL, "Cell", "Atlas char width / height (px)", cell, 2,
                1.0f, 1.0f, 256.0f, 32.0f, 0.0f, "%.0f", "WH")) {
            charW = cell[0];
            charH = cell[1];
            changed = true;
        }
        insp::PropRow r(ICON_FA_TABLE_CELLS, "Per Row", "Atlas chars per row");
        changed |= ImGui::InputInt("##v", &perRow);
    }
    return changed;
}

const char* UiElementIcon(UIElement* element) {
    if (dynamic_cast<UIButton*>(element)) return ICON_FA_HAND_POINTER;
    if (dynamic_cast<UIText*>(element)) return ICON_FA_FONT;
    if (dynamic_cast<UISprite*>(element)) return ICON_FA_IMAGE;
    if (dynamic_cast<UIBar*>(element)) return ICON_FA_BARS_PROGRESS;
    return ICON_FA_SQUARE;
}

const char* GetUiElementTypeName(UIElement* element) {
    if (dynamic_cast<UIButton*>(element)) return "Button";
    if (dynamic_cast<UIText*>(element)) return "Text";
    if (dynamic_cast<UISprite*>(element)) return "Sprite";
    if (dynamic_cast<UIBar*>(element)) return "Bar";
    return "Element";
}

bool DrawAnchorCombo(UIAnchor* anchor, const char* id) {
    static const char* anchorNames[] = {
        "Top Left", "Top Center", "Top Right",
        "Middle Left", "Center", "Middle Right",
        "Bottom Left", "Bottom Center", "Bottom Right"
    };
    int current = static_cast<int>(*anchor);
    if (ImGui::Combo(id, &current, anchorNames, IM_ARRAYSIZE(anchorNames))) {
        *anchor = static_cast<UIAnchor>(current);
        return true;
    }
    return false;
}

void SelectCanvasElement(UIElement*& selectedUiElement, GameObject*& selectedNode, UIElement* element) {
    selectedUiElement = element;
    if (element) {
        selectedNode = nullptr;
        EngineGUI_SetEditMode(EditorEditMode::UI);
    }
}

UIButton* CreateCanvasButton() {
    UICanvas* canvas = SceneCanvas_GetCanvas();
    if (!canvas) return nullptr;
    UIButton* button = canvas->AddButton("Button", { -100.0f, -30.0f }, UIAnchor::CENTER, 200.0f, 60.0f);
    button->SetText("Button");
    button->SetTextScale(1.5f);
    return button;
}

UIText* CreateCanvasText() {
    UICanvas* canvas = SceneCanvas_GetCanvas();
    if (!canvas) return nullptr;
    UIText* text = canvas->AddText("Text", { 0.0f, 0.0f }, UIAnchor::CENTER, "Text");
    text->SetFontPath(kUIFontDefaultPath, 32.0f, 32.0f, 16);
    text->SetFontSize(1.5f);
    text->SetCharacterSpacing(32.0f);
    text->SetAlignment(TextAlign::LEFT);
    text->SetPivot({0,1});
    return text;
}

UISprite* CreateCanvasSprite() {
    UICanvas* canvas = SceneCanvas_GetCanvas();
    if (!canvas) return nullptr;
    return canvas->AddSprite("Sprite", { -32.0f, -32.0f }, UIAnchor::CENTER, 64.0f, 64.0f);
}

UIBar* CreateCanvasBar() {
    UICanvas* canvas = SceneCanvas_GetCanvas();
    if (!canvas) return nullptr;
    UIBar* bar = canvas->AddBar("Bar", { -200.0f, 80.0f }, UIAnchor::CENTER, 100.0f, "Bar");
    bar->SetCurrentValue(50.0f);
    bar->SetBarColor({ 0.2f, 0.8f, 0.3f, 1.0f });
    bar->SetBackgroundColor({ 0.15f, 0.15f, 0.15f, 1.0f });
    return bar;
}

} // namespace

namespace EngineGUIUI {

UIElement* CreateUiElement(UiCreateType type) {
    switch (type) {
    case UiCreateType::Button: return CreateCanvasButton();
    case UiCreateType::Text:   return CreateCanvasText();
    case UiCreateType::Sprite: return CreateCanvasSprite();
    case UiCreateType::Bar:    return CreateCanvasBar();
    }
    return nullptr;
}

void DrawCanvasElementHierarchy(UIElement*& selectedUiElement, GameObject*& selectedNode) {
    UICanvas* canvas = SceneCanvas_GetCanvas();
    if (!canvas) {
        ImGui::TextDisabled("No canvas");
        return;
    }

    const auto& elements = canvas->GetElements();
    if (elements.empty()) {
        ImGui::TextDisabled("No canvas elements");
        return;
    }

    for (const auto& element : elements) {
        if (!element) continue;
        ImGui::PushID(element.get());
        const bool selected = (selectedUiElement == element.get());
        std::string label = std::string(GetUiElementTypeName(element.get())) + ": " + element->GetName();
        if (ImGui::Selectable(label.c_str(), selected))
            SelectCanvasElement(selectedUiElement, selectedNode, element.get());
        ImGui::PopID();
    }
}

void DrawCanvasElementProperties(UIElement*& selectedUiElement) {
    UIElement* element = selectedUiElement;
    if (!element) return;

    static UIElement* s_uiEditElement = nullptr;
    static char s_uiNameBuf[128] = {};
    static char s_textureBuf[256] = {};
    static char s_hoverTextureBuf[256] = {};
    static char s_pressedTextureBuf[256] = {};
    static char s_fontPathBuf[256] = {};
    static char s_buttonTextBuf[256] = {};
    static char s_buttonScriptBuf[128] = {};
    static char s_buttonSceneBuf[256] = {};
    static char s_textBuf[256] = {};
    static char s_barLabelBuf[128] = {};
    static char s_uiScriptClassBuf[128] = {};

    if (s_uiEditElement != element) {
        s_uiEditElement = element;
        strncpy_s(s_uiNameBuf, element->GetName().c_str(), _TRUNCATE);
        s_textureBuf[0] = '\0';
        s_hoverTextureBuf[0] = '\0';
        s_pressedTextureBuf[0] = '\0';
        s_fontPathBuf[0] = '\0';
        s_buttonTextBuf[0] = '\0';
        s_buttonScriptBuf[0] = '\0';
        s_buttonSceneBuf[0] = '\0';
        s_textBuf[0] = '\0';
        s_barLabelBuf[0] = '\0';
        s_uiScriptClassBuf[0] = '\0';
        strncpy_s(s_uiScriptClassBuf, element->GetScriptClass().c_str(), _TRUNCATE);
        if (auto* sprite = dynamic_cast<UISprite*>(element))
            strncpy_s(s_textureBuf, sprite->GetTexturePath().c_str(), _TRUNCATE);
        if (auto* text = dynamic_cast<UIText*>(element)) {
            strncpy_s(s_textBuf, text->GetText().c_str(), _TRUNCATE);
            strncpy_s(s_fontPathBuf, text->GetFontPath().c_str(), _TRUNCATE);
        }
        if (auto* button = dynamic_cast<UIButton*>(element)) {
            strncpy_s(s_buttonTextBuf, button->GetText().c_str(), _TRUNCATE);
            strncpy_s(s_buttonScriptBuf, button->GetAction().c_str(), _TRUNCATE);
            strncpy_s(s_buttonSceneBuf, button->GetActionScene().c_str(), _TRUNCATE);
            strncpy_s(s_textureBuf, button->GetTexturePath().c_str(), _TRUNCATE);
            strncpy_s(s_hoverTextureBuf, button->GetHoverTexturePath().c_str(), _TRUNCATE);
            strncpy_s(s_pressedTextureBuf, button->GetPressedTexturePath().c_str(), _TRUNCATE);
            strncpy_s(s_fontPathBuf, UIButton::GetFontPath().c_str(), _TRUNCATE);
        }
        if (auto* bar = dynamic_cast<UIBar*>(element))
            strncpy_s(s_barLabelBuf, bar->GetLabel().c_str(), _TRUNCATE);
    }

    ImGui::PushItemWidth(-FLT_MIN);
    insp::SetAccent(insp::Accent::Script);

    // Name + visibility.
    {
        insp::PropRow row(UiElementIcon(element), GetUiElementTypeName(element));
        if (ImGui::InputTextWithHint("##v", "Name", s_uiNameBuf, sizeof(s_uiNameBuf),
                ImGuiInputTextFlags_EnterReturnsTrue))
            element->SetName(s_uiNameBuf);
    }
    bool visible = element->IsVisible();
    insp::Toggle visToggle[] = { { visible ? ICON_FA_EYE : ICON_FA_EYE_SLASH, "Visible", &visible } };
    if (insp::ToggleRow(visToggle, 1))
        element->SetVisible(visible);

    if (UICanvas* canvas = SceneCanvas_GetCanvas()) {
        auto undoableMove = [&](bool (UICanvas::*moveFn)(UIElement*)) {
            const int oldIndex = canvas->GetElementIndex(element);
            if ((canvas->*moveFn)(element)) {
                EditorUndo_PushUiReorder(element, oldIndex, canvas->GetElementIndex(element));
                EngineGUI_MarkSceneDirty();
            }
        };
        insp::RowLabel(ICON_FA_SORT, "Order", "Later = drawn on top");
        static const char* const kOrderIcons[] = { ICON_FA_ANGLES_DOWN, ICON_FA_ARROW_DOWN,
                                                   ICON_FA_ARROW_UP, ICON_FA_ANGLES_UP };
        static const char* const kOrderTips[] = { "Back", "Down", "Up", "Top" };
        switch (insp::IconButtonRow("uiorder", kOrderIcons, kOrderTips, 4)) {
        case 0: undoableMove(&UICanvas::MoveElementToBack); break;
        case 1: undoableMove(&UICanvas::MoveElementBackward); break;
        case 2: undoableMove(&UICanvas::MoveElementForward); break;
        case 3: undoableMove(&UICanvas::MoveElementToFront); break;
        default: break;
        }
    }

    // Layout.
    insp::SubHeading(ICON_FA_UP_DOWN_LEFT_RIGHT, "Layout", "Canvas design space 1920 x 1080", false);
    {
        UIAnchor anchor = element->GetAnchor();
        insp::PropRow row(ICON_FA_ANCHOR, "Anchor");
        if (DrawAnchorCombo(&anchor, "##v"))
            element->SetAnchor(anchor);
    }
    {
        DirectX::XMFLOAT2 pivot = element->GetPivot();
        if (insp::PivotRow(&pivot.x, element))
            element->SetPivot(pivot);
    }
    {
        DirectX::XMFLOAT2 pos = element->GetPosition();
        if (insp::DragVecN(ICON_FA_LOCATION_DOT, "Pos", "Position (px)", &pos.x, 2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, "%.1f"))
            element->SetPosition(pos);
    }

    if (auto* sprite = dynamic_cast<UISprite*>(element)) {
        insp::SubHeading(ICON_FA_IMAGE, "Sprite",
            sprite->HasExplicitPivot() && sprite->GetRotation() != 0 ? "Rotated: resize with Size" : nullptr, false);
        float size[2] = { sprite->GetWidth(), sprite->GetHeight() };
        if (insp::DragVecN(ICON_FA_EXPAND, "Size", "Width / height (px)", size, 2, 1.0f, 1.0f, 4096.0f, 64.0f, 0.0f, "%.1f", "WH"))
            sprite->SetSize(size[0], size[1]);
        float rotation = sprite->GetRotation();
        { insp::PropRow r(ICON_FA_ROTATE, "Rot", "Rotation (deg)");
          if (ImGui::DragFloat("##v", &rotation, 1.0f)) sprite->SetRotation(rotation); }
        DirectX::XMFLOAT4 color = sprite->GetColor();
        { insp::PropRow r(ICON_FA_PALETTE, "Color");
          if (ImGui::ColorEdit4("##v", &color.x)) sprite->SetColor(color); }
        if (PathRow(ICON_FA_IMAGE, "Texture", nullptr, s_textureBuf, sizeof(s_textureBuf), kTextureExts, IM_ARRAYSIZE(kTextureExts)))
            sprite->SetTexturePath(s_textureBuf);
    }
    else if (auto* text = dynamic_cast<UIText*>(element)) {
        insp::SubHeading(ICON_FA_FONT, "Text");
        { insp::PropRow r(ICON_FA_I_CURSOR, "Text");
          if (ImGui::InputText("##v", s_textBuf, sizeof(s_textBuf))) text->SetText(s_textBuf); }

        const bool pivotSet = text->HasExplicitPivot();
        insp::RowLabel(ICON_FA_ALIGN_LEFT, "Align", pivotSet ? "Pivot positions the text block" : nullptr);
        static const char* const kAlignIcons[] = { ICON_FA_ALIGN_LEFT, ICON_FA_ALIGN_CENTER, ICON_FA_ALIGN_RIGHT };
        static const char* const kAlignTips[] = { "Left", "Center", "Right" };
        int align = static_cast<int>(text->GetAlignment());
        ImGui::BeginDisabled(pivotSet);
        if (insp::ChoiceBar("align", kAlignIcons, kAlignTips, 3, &align))
            text->SetAlignment(static_cast<TextAlign>(align));
        ImGui::EndDisabled();

        float fontSize = text->GetFontSize();
        { insp::PropRow r(ICON_FA_TEXT_HEIGHT, "Size", "Font scale");
          if (ImGui::DragFloat("##v", &fontSize, 0.1f, 0.1f, 10.0f)) text->SetFontSize(fontSize); }
        float charSpacing = text->GetCharacterSpacing();
        { insp::PropRow r(ICON_FA_ARROWS_LEFT_RIGHT, "Spacing", "Advance per character (>= char width)");
          if (ImGui::DragFloat("##v", &charSpacing, 0.5f, 0.0f, 128.0f)) text->SetCharacterSpacing(charSpacing); }
        DirectX::XMFLOAT4 color = text->GetColor();
        { insp::PropRow r(ICON_FA_PALETTE, "Color");
          if (ImGui::ColorEdit4("##v", &color.x)) text->SetColor(color); }

        float charWidth = text->GetFontCharWidth();
        float charHeight = text->GetFontCharHeight();
        int charsPerRow = text->GetFontCharsPerRow();
        if (DrawFontRows(s_fontPathBuf, sizeof(s_fontPathBuf), charWidth, charHeight, charsPerRow) &&
            s_fontPathBuf[0] != '\0')
            text->SetFontPath(s_fontPathBuf, charWidth, charHeight, charsPerRow);
    }
    else if (auto* button = dynamic_cast<UIButton*>(element)) {
        insp::SubHeading(ICON_FA_HAND_POINTER, "Button");
        float size[2] = { button->GetWidth(), button->GetHeight() };
        if (insp::DragVecN(ICON_FA_EXPAND, "Size", "Width / height (px)", size, 2, 1.0f, 1.0f, 4096.0f, 64.0f, 0.0f, "%.1f", "WH"))
            button->SetSize(size[0], size[1]);
        { insp::PropRow r(ICON_FA_I_CURSOR, "Text");
          if (ImGui::InputText("##v", s_buttonTextBuf, sizeof(s_buttonTextBuf))) button->SetText(s_buttonTextBuf); }
        float textScale = button->GetTextScale();
        { insp::PropRow r(ICON_FA_TEXT_HEIGHT, "Size", "Text scale");
          if (ImGui::DragFloat("##v", &textScale, 0.1f, 0.1f, 10.0f)) button->SetTextScale(textScale); }

        insp::SubHeading(ICON_FA_PALETTE, "Colors");
        auto colorRow = [](const char* icon, const char* word, const char* tip, DirectX::XMFLOAT4 c, void (UIButton::*set)(const DirectX::XMFLOAT4&), UIButton* b) {
            insp::PropRow r(icon, word, tip);
            if (ImGui::ColorEdit4("##v", &c.x)) (b->*set)(c);
        };
        colorRow(ICON_FA_SQUARE, "BG", "Background", button->GetColor(), &UIButton::SetColor, button);
        colorRow(ICON_FA_HAND_POINTER, "Hover", "Hover background", button->GetHoverColor(), &UIButton::SetHoverColor, button);
        colorRow(ICON_FA_CIRCLE_DOT, "Pressed", "Pressed background", button->GetPressedColor(), &UIButton::SetPressedColor, button);
        colorRow(ICON_FA_FONT, "Text", "Text color", button->GetTextColor(), &UIButton::SetTextColor, button);
        colorRow(ICON_FA_FONT, "Text Hov", "Text hover color", button->GetTextHoverColor(), &UIButton::SetTextHoverColor, button);

        insp::SubHeading(ICON_FA_IMAGE, "Textures");
        if (PathRow(ICON_FA_IMAGE, "Normal", "Texture", s_textureBuf, sizeof(s_textureBuf), kTextureExts, IM_ARRAYSIZE(kTextureExts)))
            button->SetTexturePath(s_textureBuf);
        if (PathRow(ICON_FA_HAND_POINTER, "Hover", "Hover texture", s_hoverTextureBuf, sizeof(s_hoverTextureBuf), kTextureExts, IM_ARRAYSIZE(kTextureExts)))
            button->SetHoverTexturePath(s_hoverTextureBuf);
        if (PathRow(ICON_FA_CIRCLE_DOT, "Pressed", "Pressed texture", s_pressedTextureBuf, sizeof(s_pressedTextureBuf), kTextureExts, IM_ARRAYSIZE(kTextureExts)))
            button->SetPressedTexturePath(s_pressedTextureBuf);

        float charWidth = UIButton::GetFontCharWidth();
        float charHeight = UIButton::GetFontCharHeight();
        int charsPerRow = UIButton::GetFontCharsPerRow();
        if (DrawFontRows(s_fontPathBuf, sizeof(s_fontPathBuf), charWidth, charHeight, charsPerRow,
                "Shared by all buttons") && s_fontPathBuf[0] != '\0')
            UIButton::SetFontPath(s_fontPathBuf, charWidth, charHeight, charsPerRow);

        insp::SubHeading(ICON_FA_BOLT, "Click");
        bool playOnLoad = button->GetActionPlayOnLoad();
        {
            static const char* const kActions[] = { "", "LoadScene", "ExitGame" };
            const char* current = button->GetAction().empty() ? "None" : button->GetAction().c_str();
            insp::PropRow r(ICON_FA_BOLT, "Action", "Built-in action or script name");
            if (ImGui::BeginCombo("##v", current)) {
                for (const char* action : kActions) {
                    const bool selected = button->GetAction() == action;
                    if (ImGui::Selectable(action[0] ? action : "None", selected)) {
                        strncpy_s(s_buttonScriptBuf, action, _TRUNCATE);
                        button->SetAction(action, s_buttonSceneBuf, playOnLoad);
                        SceneCanvas_BindButtonAction(button);
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        {
            insp::PropRow r(ICON_FA_CODE, "Custom", "Custom action / script (Enter)");
            if (ImGui::InputText("##v", s_buttonScriptBuf, sizeof(s_buttonScriptBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                button->SetAction(s_buttonScriptBuf, s_buttonSceneBuf, playOnLoad);
                SceneCanvas_BindButtonAction(button);
            }
        }
        {
            insp::PropRow r(ICON_FA_DOOR_OPEN, "Scene", "Scene to load (Enter)");
            if (ImGui::InputText("##v", s_buttonSceneBuf, sizeof(s_buttonSceneBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                button->SetAction(s_buttonScriptBuf, s_buttonSceneBuf, playOnLoad);
                SceneCanvas_BindButtonAction(button);
            }
        }
        insp::Toggle playToggle[] = { { ICON_FA_PLAY, "Play after load", &playOnLoad } };
        if (insp::ToggleRow(playToggle, 1)) {
            button->SetAction(s_buttonScriptBuf, s_buttonSceneBuf, playOnLoad);
            SceneCanvas_BindButtonAction(button);
        }
    }
    else if (auto* bar = dynamic_cast<UIBar*>(element)) {
        insp::SubHeading(ICON_FA_BARS_PROGRESS, "Bar");
        { insp::PropRow r(ICON_FA_I_CURSOR, "Label");
          if (ImGui::InputText("##v", s_barLabelBuf, sizeof(s_barLabelBuf))) bar->SetLabel(s_barLabelBuf); }
        float currentValue = bar->GetCurrentValue();
        float maxValue = bar->GetMaxValue();
        { insp::PropRow r(ICON_FA_PERCENT, "Value", "Current value");
          if (ImGui::DragFloat("##v", &currentValue, 1.0f)) bar->SetCurrentValue(currentValue); }
        { insp::PropRow r(ICON_FA_ANGLES_UP, "Max");
          if (ImGui::DragFloat("##v", &maxValue, 1.0f, 0.001f, 100000.0f)) bar->SetMaxValue(maxValue); }
        int barSize[2] = { bar->GetBarWidth(), bar->GetBarHeight() };
        { insp::PropRow r(ICON_FA_EXPAND, "Size", "Width / height (px)");
          if (ImGui::DragInt2("##v", barSize, 1.0f, 1, 8192))
              bar->SetBarSize((std::max)(1, barSize[0]), (std::max)(1, barSize[1])); }
        bool showText = bar->GetShowText();
        bool showValue = bar->GetShowValue();
        insp::Toggle toggles[] = {
            { ICON_FA_FONT, "Label", &showText },
            { ICON_FA_PERCENT, "Value", &showValue },
        };
        if (insp::ToggleRow(toggles, 2)) {
            bar->SetShowText(showText);
            bar->SetShowValue(showValue);
        }
        float spacing = bar->GetCharacterSpacing();
        { insp::PropRow r(ICON_FA_ARROWS_LEFT_RIGHT, "Spacing", "Character spacing");
          if (ImGui::DragFloat("##v", &spacing, 0.5f, 0.0f, 100.0f)) bar->SetCharacterSpacing(spacing); }
        DirectX::XMFLOAT4 bg = bar->GetBackgroundColor();
        { insp::PropRow r(ICON_FA_SQUARE, "BG", "Background");
          if (ImGui::ColorEdit4("##v", &bg.x)) bar->SetBackgroundColor(bg); }
        DirectX::XMFLOAT4 fill = bar->GetBarColor();
        { insp::PropRow r(ICON_FA_FILL, "Fill");
          if (ImGui::ColorEdit4("##v", &fill.x)) bar->SetBarColor(fill); }
        DirectX::XMFLOAT4 textColor = bar->GetTextColor();
        { insp::PropRow r(ICON_FA_FONT, "Text", "Text color");
          if (ImGui::ColorEdit4("##v", &textColor.x)) bar->SetTextColor(textColor); }
    }

    // Attached UI script.
    insp::SubHeading(ICON_FA_CODE, "Script");
    {
        const std::vector<std::string> uiScripts = UICanvasScriptFactory::Instance().GetRegisteredNames();
        insp::PropRow r(ICON_FA_CODE, "Class", "UICanvasScript class");
        if (ImGui::BeginCombo("##v", s_uiScriptClassBuf[0] ? s_uiScriptClassBuf : "None")) {
            if (ImGui::Selectable("None", s_uiScriptClassBuf[0] == '\0')) {
                s_uiScriptClassBuf[0] = '\0';
                element->SetScriptClass("");
            }
            for (const std::string& name : uiScripts) {
                const bool selected = element->GetScriptClass() == name;
                if (ImGui::Selectable(name.c_str(), selected)) {
                    strncpy_s(s_uiScriptClassBuf, name.c_str(), _TRUNCATE);
                    element->SetScriptClass(name);
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
    {
        insp::PropRow r(ICON_FA_PEN, "Custom", "Class name not in the list (Enter)");
        if (ImGui::InputText("##v", s_uiScriptClassBuf, sizeof(s_uiScriptClassBuf), ImGuiInputTextFlags_EnterReturnsTrue))
            element->SetScriptClass(s_uiScriptClassBuf);
    }

    ImGui::Spacing();
    ImGui::Separator();
    if (insp::WideButton(ICON_FA_TRASH_CAN, "Delete", "Delete UI element")) {
        EditorUndo_DiscardUiEdit();
        EditorUndo_PushUiDelete(element);
        if (UICanvas* canvas = SceneCanvas_GetCanvas())
            canvas->RemoveElement(element);
        selectedUiElement = nullptr;
        s_uiEditElement = nullptr;
        EngineGUI_MarkSceneDirty();
    }
    ImGui::PopItemWidth();
}

} // namespace EngineGUIUI
