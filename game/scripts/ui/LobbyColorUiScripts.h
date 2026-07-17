#pragma once

#include "engine/scripting/ui_canvas_script.h"

class LobbyColorPreviewScript : public UICanvasScript {
public:
    void OnCreate(UIElement& owner) override;
    void OnUpdate(UIElement& owner, float dt) override;

private:
    int m_slot = -1;
};
