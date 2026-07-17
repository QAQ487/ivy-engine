#pragma once

class GameObject;

namespace TransformGizmo {

void BeginFrame();
void HandleHotkeys();
void SetVisible(bool visible);
bool IsVisible();

void Draw(GameObject* selected);
bool IsInteracting();

void DrawToolbarControls();

} // namespace TransformGizmo
