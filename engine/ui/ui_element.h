/**===========================================================================
 File   :  [ui_element.h]
 Author : Copilot Agent
 Date   : 2025/11/02
 Brief  : Base class for UI elements that can be rendered as overlays
 ===========================================================================**/

#ifndef UI_ELEMENT_H
#define UI_ELEMENT_H

#include "libraries/nlohmann/json.hpp"
#include "ui_element_script_host.h"
#include <DirectXMath.h>
#include <memory>
#include <string>

/**
 * Anchor point for positioning UI elements
 */
enum class UIAnchor {
    TOP_LEFT,
    TOP_CENTER,
    TOP_RIGHT,
    CENTER_LEFT,
    CENTER,
    CENTER_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_CENTER,
    BOTTOM_RIGHT
};

struct UIScreenRect {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

/**
 * Base class for all UI elements in the game.
 * Provides a common interface for updating and drawing UI components.
 * Position is relative to the anchor point with (0,0) at the anchor.
 */
class UIElement {
protected:
    bool m_isVisible;
    DirectX::XMFLOAT2 m_position;  // Top-left offset from anchor in design space
    UIAnchor m_anchor;              // Anchor point for positioning
    std::string m_name;             // Unique identifier for the element
    std::unique_ptr<UIElementScriptHost> m_scriptHost;
    
public:
    UIElement(const std::string& name, const DirectX::XMFLOAT2& position, UIAnchor anchor = UIAnchor::CENTER)
        : m_name(name), m_position(position), m_anchor(anchor), m_isVisible(true) {}
    
    virtual ~UIElement();
    
    // Initialize resources
    virtual void Initialize() = 0;
    
    // Update the UI element
    virtual void Update(double elapsed_time) = 0;
    
    // Draw the UI element
    virtual void Draw() = 0;
    
    // Cleanup resources
    virtual void Finalize() = 0;
    
    // Getters and setters
    void SetVisible(bool visible) { m_isVisible = visible; }
    bool IsVisible() const { return m_isVisible; }
    
    void SetPosition(const DirectX::XMFLOAT2& position) { m_position = position; }
    DirectX::XMFLOAT2 GetPosition() const { return m_position; }
    
    void SetAnchor(UIAnchor anchor) { m_anchor = anchor; }
    UIAnchor GetAnchor() const { return m_anchor; }
    
    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }
    
    // Convert relative position to screen coordinates (render-viewport space).
    DirectX::XMFLOAT2 GetScreenPosition() const;
    DirectX::XMFLOAT2 GetScreenPosition(float screenWidth, float screenHeight) const;
    // Anchor reference point in viewport-local scaled space (no position offset).
    DirectX::XMFLOAT2 GetAnchorScreenPosition() const;
    DirectX::XMFLOAT2 GetAnchorScreenPosition(float screenWidth, float screenHeight) const;

    // Bounds in viewport-local scaled draw space (matches Sprite_Draw coordinates).
    virtual bool GetScreenRect(UIScreenRect& out) const = 0;
    bool HitTest(float viewportX, float viewportY) const;

    void LoadScriptFromJson(const nlohmann::json& scriptJson);
    nlohmann::json SaveScriptToJson() const;
    void SetScriptClass(const std::string& scriptClass);
    const std::string& GetScriptClass() const;
    void UpdateAttachedScript(double elapsed_time);
    void InvokeAttachedScriptOnClick();
    void TeardownAttachedScript();
    void HotReloadAttachedScript();
};

// Scale from design (1920x1080) space to the active render viewport.
DirectX::XMFLOAT2 UIGetLayoutScale();
DirectX::XMFLOAT2 UIGetDesignSize();
DirectX::XMFLOAT2 ScreenDeltaToDesignDelta(float dx, float dy);

#endif // UI_ELEMENT_H
