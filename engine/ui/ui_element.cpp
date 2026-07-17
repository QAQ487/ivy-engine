/**===========================================================================
 File   :  [ui_element.cpp]
 Author : Copilot Agent
 Date   : 2025/11/02
 Brief  : Implementation of UIElement base class
 ===========================================================================**/

#include "ui_element.h"
#include "ui_element_script_host.h"
#include "engine/core/engine_gui.h"
#include "engine/rendering/d3d11/renderer.h"
#include "main.h"

namespace {

void GetAnchorDesignPosition(UIAnchor anchor, float screenWidth, float screenHeight,
    float& anchorX, float& anchorY)
{
    anchorX = 0.0f;
    anchorY = 0.0f;

    switch (anchor) {
    case UIAnchor::TOP_LEFT:
        break;
    case UIAnchor::TOP_CENTER:
        anchorX = screenWidth / 2.0f;
        break;
    case UIAnchor::TOP_RIGHT:
        anchorX = screenWidth;
        break;
    case UIAnchor::CENTER_LEFT:
        anchorY = screenHeight / 2.0f;
        break;
    case UIAnchor::CENTER:
        anchorX = screenWidth / 2.0f;
        anchorY = screenHeight / 2.0f;
        break;
    case UIAnchor::CENTER_RIGHT:
        anchorX = screenWidth;
        anchorY = screenHeight / 2.0f;
        break;
    case UIAnchor::BOTTOM_LEFT:
        anchorY = screenHeight;
        break;
    case UIAnchor::BOTTOM_CENTER:
        anchorX = screenWidth / 2.0f;
        anchorY = screenHeight;
        break;
    case UIAnchor::BOTTOM_RIGHT:
        anchorX = screenWidth;
        anchorY = screenHeight;
        break;
    }
}

} // namespace

UIElement::~UIElement() {
    if (m_scriptHost)
        m_scriptHost->Teardown(*this);
}

void UIElement::LoadScriptFromJson(const nlohmann::json& scriptJson) {
    if (!m_scriptHost)
        m_scriptHost = std::make_unique<UIElementScriptHost>();
    m_scriptHost->LoadFromJson(scriptJson);
}

nlohmann::json UIElement::SaveScriptToJson() const {
    return m_scriptHost ? m_scriptHost->SaveToJson() : nlohmann::json();
}

void UIElement::SetScriptClass(const std::string& scriptClass) {
    if (!m_scriptHost)
        m_scriptHost = std::make_unique<UIElementScriptHost>();
    m_scriptHost->SetScriptClass(scriptClass);
}

const std::string& UIElement::GetScriptClass() const {
    static const std::string kEmpty;
    return m_scriptHost ? m_scriptHost->GetScriptClass() : kEmpty;
}

void UIElement::UpdateAttachedScript(double elapsed_time) {
    if (m_scriptHost)
        m_scriptHost->Update(*this, elapsed_time);
}

void UIElement::InvokeAttachedScriptOnClick() {
    if (m_scriptHost)
        m_scriptHost->InvokeOnClick(*this);
}

void UIElement::TeardownAttachedScript() {
    if (m_scriptHost)
        m_scriptHost->Teardown(*this);
}

DirectX::XMFLOAT2 UIGetDesignSize() {
    return {
        static_cast<float>(SCREEN_WIDTH),
        static_cast<float>(SCREEN_HEIGHT)
    };
}

DirectX::XMFLOAT2 UIGetLayoutScale() {
    const DirectX::XMFLOAT2 design = UIGetDesignSize();
    if (design.x <= 0.0f || design.y <= 0.0f)
        return { 1.0f, 1.0f };

    float viewportW = GetRenderViewportWidth();
    float viewportH = GetRenderViewportHeight();

    // During editor Update (before SetRenderViewport), the D3D viewport may still
    // be fullscreen while UI is laid out in the inset game viewport.
    if (!EngineGUI_IsPlaying()) {
        float vx = 0.0f, vy = 0.0f, vw = 0.0f, vh = 0.0f;
        EngineGUI_GetGameViewportRect(vx, vy, vw, vh);
        if (vw > 0.0f && vh > 0.0f) {
            viewportW = vw;
            viewportH = vh;
        }
    }

    return { viewportW / design.x, viewportH / design.y };
}

DirectX::XMFLOAT2 UIElement::GetScreenPosition() const {
    const DirectX::XMFLOAT2 design = UIGetDesignSize();
    return GetScreenPosition(design.x, design.y);
}

DirectX::XMFLOAT2 UIElement::GetAnchorScreenPosition() const {
    const DirectX::XMFLOAT2 design = UIGetDesignSize();
    return GetAnchorScreenPosition(design.x, design.y);
}

DirectX::XMFLOAT2 UIElement::GetAnchorScreenPosition(float screenWidth, float screenHeight) const {
    float anchorX = 0.0f;
    float anchorY = 0.0f;
    GetAnchorDesignPosition(m_anchor, screenWidth, screenHeight, anchorX, anchorY);

    const DirectX::XMFLOAT2 layoutScale = UIGetLayoutScale();
    return { anchorX * layoutScale.x, anchorY * layoutScale.y };
}

DirectX::XMFLOAT2 UIElement::GetScreenPosition(float screenWidth, float screenHeight) const {
    float anchorX = 0.0f;
    float anchorY = 0.0f;
    GetAnchorDesignPosition(m_anchor, screenWidth, screenHeight, anchorX, anchorY);

    // Position is the element top-left offset from the anchor in design space.
    DirectX::XMFLOAT2 screenPos;
    screenPos.x = anchorX + m_position.x;
    screenPos.y = anchorY + m_position.y;

    const DirectX::XMFLOAT2 layoutScale = UIGetLayoutScale();
    screenPos.x *= layoutScale.x;
    screenPos.y *= layoutScale.y;

    return screenPos;
}

DirectX::XMFLOAT2 ScreenDeltaToDesignDelta(float dx, float dy) {
    const DirectX::XMFLOAT2 layoutScale = UIGetLayoutScale();
    return {
        layoutScale.x > 0.0f ? dx / layoutScale.x : 0.0f,
        layoutScale.y > 0.0f ? dy / layoutScale.y : 0.0f
    };
}

bool UIElement::HitTest(float viewportX, float viewportY) const {
    if (!m_isVisible)
        return false;

    UIScreenRect rect;
    if (!GetScreenRect(rect))
        return false;

    return viewportX >= rect.left && viewportX <= rect.right
        && viewportY >= rect.top && viewportY <= rect.bottom;
}
