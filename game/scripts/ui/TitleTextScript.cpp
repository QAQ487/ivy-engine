#include "TitleTextScript.h"

#include "engine/scripting/ui_canvas_script_register.h"
#include "engine/ui/ui_element.h"
#include <cmath>

REGISTER_UI_SCRIPT(TitleTextScript)

void TitleTextScript::OnCreate(UIElement& owner) {
    m_baseY = owner.GetPosition().y;
    m_waveTime = 0.0f;
}

void TitleTextScript::OnUpdate(UIElement& owner, float dt) {
    m_waveTime += dt;
    DirectX::XMFLOAT2 pos = owner.GetPosition();
    pos.y = m_baseY + std::sin(m_waveTime * m_speed) * m_amplitude;
    owner.SetPosition(pos);
}
