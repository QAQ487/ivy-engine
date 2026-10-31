// editor_ui_panel.h
// Editor panel for inspecting/editing UI canvases.

#pragma once

class GameObject;
class UIElement;

namespace EngineGUIUI {

enum class UiCreateType {
    Button,
    Text,
    Sprite,
    Bar
};

UIElement* CreateUiElement(UiCreateType type);
void DrawCanvasElementHierarchy(UIElement*& selectedUiElement, GameObject*& selectedNode);
void DrawCanvasElementProperties(UIElement*& selectedUiElement);

} // namespace EngineGUIUI
