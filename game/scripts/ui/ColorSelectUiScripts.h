#pragma once

#include "engine/scripting/ui_canvas_script.h"

class ColorPreviewScript : public UICanvasScript {
public:
    void OnCreate(UIElement& owner) override;
    void OnUpdate(UIElement& owner, float dt) override;
};

class ColorHighlightScript : public UICanvasScript {
public:
    void OnUpdate(UIElement& owner, float dt) override;
};

class ColorEnemyButtonScript : public UICanvasScript {
public:
    void OnCreate(UIElement& owner) override;
    void OnUpdate(UIElement& owner, float dt) override;

private:
    bool m_isEnemyButton = false;
};
