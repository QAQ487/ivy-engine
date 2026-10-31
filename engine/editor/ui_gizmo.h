// ui_gizmo.h
// Editor gizmo for UI element transforms.

#pragma once

class UIElement;

namespace UiGizmo {

void HandleInput(UIElement* selected);
void Draw(UIElement* selected);
bool IsInteracting();
bool WouldCaptureMouse(UIElement* selected, float viewportX, float viewportY);

} // namespace UiGizmo
