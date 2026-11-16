// editor_ui_panel.cpp
// Editor panel for inspecting/editing UI canvases.

#include "editor_ui_panel.h"
#include "engine/core/engine_gui.h"
#include "engine/core/utf8_path.h"
#include "engine/editor/editor_undo.h"
#include "engine/editor/pivot_presets.h"

#include "imgui/imgui.h"
#include "../ui/scene_canvas.h"
#include "../ui/ui_bar.h"
#include "../ui/ui_button.h"
#include "../ui/ui_canvas.h"
#include "../ui/ui_sprite.h"
#include "../ui/ui_text.h"
#include "../scripting/ui_canvas_script_factory.h"
#include "engine/ecs/game_object.h"

#include <algorithm>
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

bool AssetPathHasTextureExtension(const char* path) {
    const std::string ext = LowerAssetExtension(path);
    for (const char* textureExt : kTextureExts) {
        if (ext == textureExt)
            return true;
    }
    return false;
}

bool AcceptTexturePathDrop(std::string& outPath) {
    bool accepted = false;
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            const char* path = static_cast<const char*>(payload->Data);
            if (path && AssetPathHasTextureExtension(path)) {
                outPath = path;
                accepted = true;
            }
        }
        ImGui::EndDragDropTarget();
    }
    return accepted;
}

bool AssetPathHasFontExtension(const char* path) {
    const std::string ext = LowerAssetExtension(path);
    for (const char* fontExt : kFontExts) {
        if (ext == fontExt)
            return true;
    }
    return false;
}

bool DrawFontPathInput(const char* id, char* buf, size_t bufSize) {
    bool changed = ImGui::InputText(id, buf, bufSize, ImGuiInputTextFlags_EnterReturnsTrue);
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            const char* path = static_cast<const char*>(payload->Data);
            if (path && AssetPathHasFontExtension(path)) {
                strncpy_s(buf, bufSize, path, _TRUNCATE);
                changed = true;
            }
        }
        ImGui::EndDragDropTarget();
    }
    return changed;
}

bool DrawTexturePathInput(const char* id, char* buf, size_t bufSize) {
    bool changed = ImGui::InputText(id, buf, bufSize, ImGuiInputTextFlags_EnterReturnsTrue);
    std::string droppedPath;
    if (AcceptTexturePathDrop(droppedPath)) {
        strncpy_s(buf, bufSize, droppedPath.c_str(), _TRUNCATE);
        changed = true;
    }
    return changed;
}

const char* GetUiElementTypeName(UIElement* element) {
    if (dynamic_cast<UIButton*>(element)) return "Button";
    if (dynamic_cast<UIText*>(element)) return "Text";
    if (dynamic_cast<UISprite*>(element)) return "Sprite";
    if (dynamic_cast<UIBar*>(element)) return "Bar";
    return "Element";
}

bool DrawTextAlignCombo(TextAlign* align, const char* id) {
    static const char* alignNames[] = { "LEFT", "CENTER", "RIGHT" };
    int current = static_cast<int>(*align);
    if (ImGui::Combo(id, &current, alignNames, IM_ARRAYSIZE(alignNames))) {
        *align = static_cast<TextAlign>(current);
        return true;
    }
    return false;
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
    text->SetFontPath("rom/font/LemonSlice.ttf", 32.0f, 32.0f, 16);
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

    ImGui::PushTextWrapPos(0);
    ImGui::PushItemWidth(-1);
    ImGui::Text("%s: %s", GetUiElementTypeName(element), element->GetName().c_str());
    ImGui::Separator();

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

    ImGui::SetNextItemWidth(-1);
    ImGui::TextUnformatted("Name");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##Name##UIName", s_uiNameBuf, sizeof(s_uiNameBuf),
        ImGuiInputTextFlags_EnterReturnsTrue))
        element->SetName(s_uiNameBuf);

    bool visible = element->IsVisible();
    if (ImGui::Checkbox("Visible", &visible))
        element->SetVisible(visible);

    if (UICanvas* canvas = SceneCanvas_GetCanvas()) {
        ImGui::Text("Draw Order");
        auto undoableMove = [&](bool (UICanvas::*moveFn)(UIElement*)) {
            const int oldIndex = canvas->GetElementIndex(element);
            if ((canvas->*moveFn)(element)) {
                EditorUndo_PushUiReorder(element, oldIndex, canvas->GetElementIndex(element));
                EngineGUI_MarkSceneDirty();
            }
        };
        const float orderWidth=(std::max)(1.0f,(ImGui::GetContentRegionAvail().x-3*ImGui::GetStyle().ItemSpacing.x)/4);
        if (ImGui::Button("Back", ImVec2(orderWidth, 0)))
            undoableMove(&UICanvas::MoveElementToBack);
        ImGui::SameLine();
        if (ImGui::Button("Down", ImVec2(orderWidth, 0)))
            undoableMove(&UICanvas::MoveElementBackward);
        ImGui::SameLine();
        if (ImGui::Button("Up", ImVec2(orderWidth, 0)))
            undoableMove(&UICanvas::MoveElementForward);
        ImGui::SameLine();
        if (ImGui::Button("Top", ImVec2(orderWidth, 0)))
            undoableMove(&UICanvas::MoveElementToFront);
        ImGui::TextDisabled("Later elements draw on top.");
    }

    UIAnchor anchor = element->GetAnchor();
    ImGui::Text("Anchor");
    ImGui::SetNextItemWidth(-1);
    if (DrawAnchorCombo(&anchor, "##UIAnchor"))
        element->SetAnchor(anchor);

    DirectX::XMFLOAT2 pivot = element->GetPivot();
    if (DrawPivotPreset("Pivot", pivot, element))
        element->SetPivot(pivot);

    DirectX::XMFLOAT2 pos = element->GetPosition();
    ImGui::Text("Position");
    ImGui::Text("X:"); ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat("##UIPosX", &pos.x, 1.0f)) element->SetPosition(pos);
    ImGui::Text("Y:"); ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat("##UIPosY", &pos.y, 1.0f)) element->SetPosition(pos);

    if (auto* sprite = dynamic_cast<UISprite*>(element)) {
        ImGui::Separator();
        ImGui::Text("Sprite");
        float width = sprite->GetWidth();
        float height = sprite->GetHeight();
        ImGui::Text("Width:"); ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##SpriteW", &width, 1.0f, 1.0f, 4096.0f)) sprite->SetSize(width, height);
        ImGui::Text("Height:"); ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##SpriteH", &height, 1.0f, 1.0f, 4096.0f)) sprite->SetSize(width, height);
        if(sprite->HasExplicitPivot() && sprite->GetRotation()!=0) ImGui::TextDisabled("Rotated sprite: resize with Width / Height.");
        float rotation = sprite->GetRotation();
        ImGui::Text("Rotation:"); ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##SpriteRot", &rotation, 1.0f)) sprite->SetRotation(rotation);
        DirectX::XMFLOAT4 color = sprite->GetColor();
        ImGui::TextUnformatted("Color");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::ColorEdit4("##Color##Sprite", &color.x)) sprite->SetColor(color);
        ImGui::Text("Texture Path");
        ImGui::SetNextItemWidth(-1);
        if (DrawTexturePathInput("##SpriteTex", s_textureBuf, sizeof(s_textureBuf)))
            sprite->SetTexturePath(s_textureBuf);
    }
    else if (auto* text = dynamic_cast<UIText*>(element)) {
        ImGui::Separator();
        ImGui::Text("Text");
        ImGui::SetNextItemWidth(-1);
        ImGui::TextUnformatted("Text");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##Text##UITextValue", s_textBuf, sizeof(s_textBuf)))
            text->SetText(s_textBuf);

        TextAlign alignment = text->GetAlignment();
        ImGui::Text("Alignment");
        ImGui::SetNextItemWidth(-1);
        ImGui::BeginDisabled(text->HasExplicitPivot());
        if (DrawTextAlignCombo(&alignment, "##UITextAlign"))
            text->SetAlignment(alignment);
        ImGui::EndDisabled();
        if(text->HasExplicitPivot()) ImGui::TextDisabled("Pivot positions the text block.");

        float fontSize = text->GetFontSize();
        ImGui::Text("Font Size:"); ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##UIFontSize", &fontSize, 0.1f, 0.1f, 10.0f)) text->SetFontSize(fontSize);

        float charSpacing = text->GetCharacterSpacing();
        ImGui::Text("Char Spacing:"); ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##UITextSpacing", &charSpacing, 0.5f, 0.0f, 128.0f))
            text->SetCharacterSpacing(charSpacing);
        ImGui::TextDisabled("Advance per character; use >= char width to avoid overlap.");

        float charWidth = text->GetFontCharWidth();
        float charHeight = text->GetFontCharHeight();
        int charsPerRow = text->GetFontCharsPerRow();
        ImGui::Text("Char W:"); ImGui::SetNextItemWidth(-1);
        bool fontChanged = ImGui::DragFloat("##UITextCharW", &charWidth, 1.0f, 1.0f, 256.0f);
        ImGui::Text("Char H:"); ImGui::SetNextItemWidth(-1);
        fontChanged |= ImGui::DragFloat("##UITextCharH", &charHeight, 1.0f, 1.0f, 256.0f);
        ImGui::Text("Per Row:"); ImGui::SetNextItemWidth(-1);
        fontChanged |= ImGui::InputInt("##UITextCharsRow", &charsPerRow);
        ImGui::Text("Font Path");
        ImGui::SetNextItemWidth(-1);
        if (DrawFontPathInput("##TextFont", s_fontPathBuf, sizeof(s_fontPathBuf)))
            fontChanged = true;
        ImGui::TextDisabled("TTF uses glyph advances; char W/H/row apply to PNG atlases only.");
        if (fontChanged && s_fontPathBuf[0] != '\0')
            text->SetFontPath(s_fontPathBuf, charWidth, charHeight, charsPerRow);
        DirectX::XMFLOAT4 color = text->GetColor();
        ImGui::TextUnformatted("Color");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::ColorEdit4("##Color##Text", &color.x)) text->SetColor(color);
    }
    else if (auto* button = dynamic_cast<UIButton*>(element)) {
        ImGui::Separator();
        ImGui::Text("Button");
        float width = button->GetWidth();
        float height = button->GetHeight();
        ImGui::Text("Width:"); ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##ButtonW", &width, 1.0f, 1.0f, 4096.0f)) button->SetSize(width, height);
        ImGui::Text("Height:"); ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##ButtonH", &height, 1.0f, 1.0f, 4096.0f)) button->SetSize(width, height);

        ImGui::SetNextItemWidth(-1);
        ImGui::TextUnformatted("Text");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##Text##ButtonText", s_buttonTextBuf, sizeof(s_buttonTextBuf)))
            button->SetText(s_buttonTextBuf);

        float textScale = button->GetTextScale();
        ImGui::Text("Text Scale:"); ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##ButtonTextScale", &textScale, 0.1f, 0.1f, 10.0f)) button->SetTextScale(textScale);

        DirectX::XMFLOAT4 bgColor = button->GetColor();
        ImGui::TextUnformatted("BG Color");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::ColorEdit4("##BG Color", &bgColor.x)) button->SetColor(bgColor);
        DirectX::XMFLOAT4 hoverColor = button->GetHoverColor();
        ImGui::TextUnformatted("Hover BG");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::ColorEdit4("##Hover BG", &hoverColor.x)) button->SetHoverColor(hoverColor);
        DirectX::XMFLOAT4 pressedColor = button->GetPressedColor();
        ImGui::TextUnformatted("Pressed BG");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::ColorEdit4("##Pressed BG", &pressedColor.x)) button->SetPressedColor(pressedColor);
        DirectX::XMFLOAT4 textColor = button->GetTextColor();
        ImGui::TextUnformatted("Text Color");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::ColorEdit4("##Text Color", &textColor.x)) button->SetTextColor(textColor);
        DirectX::XMFLOAT4 textHoverColor = button->GetTextHoverColor();
        ImGui::TextUnformatted("Hover Text");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::ColorEdit4("##Hover Text", &textHoverColor.x)) button->SetTextHoverColor(textHoverColor);

        ImGui::Text("Texture Path");
        ImGui::SetNextItemWidth(-1);
        if (DrawTexturePathInput("##ButtonTex", s_textureBuf, sizeof(s_textureBuf)))
            button->SetTexturePath(s_textureBuf);
        ImGui::Text("Hover Texture Path");
        ImGui::SetNextItemWidth(-1);
        if (DrawTexturePathInput("##ButtonHoverTex", s_hoverTextureBuf, sizeof(s_hoverTextureBuf)))
            button->SetHoverTexturePath(s_hoverTextureBuf);
        ImGui::Text("Pressed Texture Path");
        ImGui::SetNextItemWidth(-1);
        if (DrawTexturePathInput("##ButtonPressedTex", s_pressedTextureBuf, sizeof(s_pressedTextureBuf)))
            button->SetPressedTexturePath(s_pressedTextureBuf);

        float charWidth = UIButton::GetFontCharWidth();
        float charHeight = UIButton::GetFontCharHeight();
        int charsPerRow = UIButton::GetFontCharsPerRow();
        bool fontChanged = false;
        ImGui::Text("Char W:"); ImGui::SetNextItemWidth(-1);
        fontChanged |= ImGui::DragFloat("##UIButtonCharW", &charWidth, 1.0f, 1.0f, 256.0f);
        ImGui::Text("Char H:"); ImGui::SetNextItemWidth(-1);
        fontChanged |= ImGui::DragFloat("##UIButtonCharH", &charHeight, 1.0f, 1.0f, 256.0f);
        ImGui::Text("Per Row:"); ImGui::SetNextItemWidth(-1);
        fontChanged |= ImGui::InputInt("##UIButtonCharsRow", &charsPerRow);
        ImGui::Text("Font Path");
        ImGui::SetNextItemWidth(-1);
        if (DrawFontPathInput("##ButtonFont", s_fontPathBuf, sizeof(s_fontPathBuf)))
            fontChanged = true;
        ImGui::TextDisabled("TTF uses glyph advances; char W/H/row apply to PNG atlases only.");
        if (fontChanged && s_fontPathBuf[0] != '\0')
            UIButton::SetFontPath(s_fontPathBuf, charWidth, charHeight, charsPerRow);

        bool playOnLoad = button->GetActionPlayOnLoad();
        const char* scriptOptions[] = { "", "LoadScene", "ExitGame" };
        const char* currentScript = (button->GetAction().empty()) ? "(none)" : button->GetAction().c_str();
        ImGui::SetNextItemWidth(-1);
        ImGui::TextUnformatted("OnClick Script");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##OnClick Script", currentScript)) {
            for (const char* script : scriptOptions) {
                const bool selected = button->GetAction() == script;
                const char* label = (script[0] == '\0') ? "(none)" : script;
                if (ImGui::Selectable(label, selected)) {
                    strncpy_s(s_buttonScriptBuf, script, _TRUNCATE);
                    button->SetAction(script, s_buttonSceneBuf, playOnLoad);
                    SceneCanvas_BindButtonAction(button);
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::Checkbox("Play after load", &playOnLoad)) {
            button->SetAction(s_buttonScriptBuf, s_buttonSceneBuf, playOnLoad);
            SceneCanvas_BindButtonAction(button);
        }
        ImGui::SetNextItemWidth(-1);
        ImGui::TextUnformatted("Custom Script");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##Custom Script##ButtonScript", s_buttonScriptBuf, sizeof(s_buttonScriptBuf),
            ImGuiInputTextFlags_EnterReturnsTrue)) {
            button->SetAction(s_buttonScriptBuf, s_buttonSceneBuf, playOnLoad);
            SceneCanvas_BindButtonAction(button);
        }
        ImGui::SetNextItemWidth(-1);
        ImGui::TextUnformatted("Scene");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##Scene##ButtonScene", s_buttonSceneBuf, sizeof(s_buttonSceneBuf),
            ImGuiInputTextFlags_EnterReturnsTrue)) {
            button->SetAction(s_buttonScriptBuf, s_buttonSceneBuf, playOnLoad);
            SceneCanvas_BindButtonAction(button);
        }
    }
    else if (auto* bar = dynamic_cast<UIBar*>(element)) {
        ImGui::Separator();
        ImGui::Text("Bar");
        ImGui::SetNextItemWidth(-1);
        ImGui::TextUnformatted("Label");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##Label##BarLabel", s_barLabelBuf, sizeof(s_barLabelBuf)))
            bar->SetLabel(s_barLabelBuf);
        float currentValue = bar->GetCurrentValue();
        float maxValue = bar->GetMaxValue();
        ImGui::Text("Current:"); ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##BarCurrent", &currentValue, 1.0f)) bar->SetCurrentValue(currentValue);
        ImGui::Text("Max:"); ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##BarMax", &maxValue, 1.0f, 0.001f, 100000.0f)) bar->SetMaxValue(maxValue);
        int barWidth = bar->GetBarWidth();
        int barHeight = bar->GetBarHeight();
        ImGui::Text("Width:"); ImGui::SetNextItemWidth(-1);
        bool sizeChanged = ImGui::InputInt("##BarWidth", &barWidth);
        ImGui::Text("Height:"); ImGui::SetNextItemWidth(-1);
        sizeChanged |= ImGui::InputInt("##BarHeight", &barHeight);
        if (sizeChanged)
            bar->SetBarSize((std::max)(1, barWidth), (std::max)(1, barHeight));
        bool showText = bar->GetShowText();
        if (ImGui::Checkbox("Show Text", &showText)) bar->SetShowText(showText);
        bool showValue = bar->GetShowValue();
        if (ImGui::Checkbox("Show Value", &showValue)) bar->SetShowValue(showValue);
        float spacing = bar->GetCharacterSpacing();
        ImGui::Text("Spacing:"); ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##BarSpacing", &spacing, 0.5f, 0.0f, 100.0f)) bar->SetCharacterSpacing(spacing);
        DirectX::XMFLOAT4 bg = bar->GetBackgroundColor();
        ImGui::TextUnformatted("BG Color");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::ColorEdit4("##BG Color##Bar", &bg.x)) bar->SetBackgroundColor(bg);
        DirectX::XMFLOAT4 fill = bar->GetBarColor();
        ImGui::TextUnformatted("Fill Color");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::ColorEdit4("##Fill Color##Bar", &fill.x)) bar->SetBarColor(fill);
        DirectX::XMFLOAT4 textColor = bar->GetTextColor();
        ImGui::TextUnformatted("Text Color");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::ColorEdit4("##Text Color##Bar", &textColor.x)) bar->SetTextColor(textColor);
    }

    ImGui::Separator();
    ImGui::Text("Attached Script");
    const std::vector<std::string> uiScripts = UICanvasScriptFactory::Instance().GetRegisteredNames();
    const char* currentUiScript = s_uiScriptClassBuf[0] ? s_uiScriptClassBuf : "(none)";
    ImGui::SetNextItemWidth(-1);
    ImGui::TextUnformatted("Script Class");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##Script Class##UiScript", currentUiScript)) {
        if (ImGui::Selectable("(none)", s_uiScriptClassBuf[0] == '\0')) {
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
    ImGui::SetNextItemWidth(-1);
    ImGui::TextUnformatted("Custom");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##Custom##UiScriptCustom", s_uiScriptClassBuf, sizeof(s_uiScriptClassBuf),
        ImGuiInputTextFlags_EnterReturnsTrue)) {
        element->SetScriptClass(s_uiScriptClassBuf);
    }

    ImGui::Separator();
    if (ImGui::Button("Delete UI Element", ImVec2(-1, 30))) {
        EditorUndo_DiscardUiEdit();
        EditorUndo_PushUiDelete(element);
        if (UICanvas* canvas = SceneCanvas_GetCanvas())
            canvas->RemoveElement(element);
        selectedUiElement = nullptr;
        s_uiEditElement = nullptr;
        EngineGUI_MarkSceneDirty();
    }
    ImGui::PopItemWidth();
    ImGui::PopTextWrapPos();
}

} // namespace EngineGUIUI
