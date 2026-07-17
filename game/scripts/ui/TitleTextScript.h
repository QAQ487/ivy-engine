#pragma once

#include "engine/scripting/ui_canvas_script.h"

class TitleTextScript : public UICanvasScript {
public:
    void OnCreate(UIElement& owner) override;
    void OnUpdate(UIElement& owner, float dt) override;

private:
    float m_baseY = 0.0f;
    float m_waveTime = 0.0f;
    float m_amplitude = 12.0f;
    float m_speed = 2.5f;
};
