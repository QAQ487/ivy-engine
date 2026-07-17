/**===========================================================================
 File   :  [ui_button.h]
 Author : Copilot Agent
 Date   : 2025/11/17
 Brief  : Interactive UI button element with keyboard and controller support
 ===========================================================================**/

#ifndef UI_BUTTON_H
#define UI_BUTTON_H

#include "ui_element.h"
#include "ui_text_renderer.h"
#include <DirectXMath.h>
#include <functional>
#include <string>

/**
 * Interactive button UI element.
 * Supports keyboard and controller navigation and click/activation.
 */
class UIButton : public UIElement {
public:
    using CallbackFunction = std::function<void()>;

private:
    int m_textureId;
    int m_textureIdHover;
    int m_textureIdPressed;
    std::string m_texturePath;
    std::string m_hoverTexturePath;
    std::string m_pressedTexturePath;
    float m_width;
    float m_height;
    DirectX::XMFLOAT4 m_color;
    DirectX::XMFLOAT4 m_colorHover;
    DirectX::XMFLOAT4 m_colorPressed;
    std::string m_text;  // Changed from wstring to string for UITextRenderer
    DirectX::XMFLOAT4 m_textColor;
    DirectX::XMFLOAT4 m_textHoverColor;
    float m_textScale;
    bool m_isHovered;
    bool m_isPressed;
    bool m_isSelected;  // For keyboard/controller navigation
    bool m_wasMouseDown;
    CallbackFunction m_onClick;
    std::string m_action;
    std::string m_actionScene;
    bool m_actionPlayOnLoad;
    
    // Manual navigation links (for custom navigation graphs)
    UIButton* m_nextUp;
    UIButton* m_nextDown;
    UIButton* m_nextLeft;
    UIButton* m_nextRight;
    
    // Shared text renderer for all buttons
    static UITextRenderer s_textRenderer;
    static bool s_textRendererInitialized;
    static std::string s_fontPath;
    static float s_fontCharWidth;
    static float s_fontCharHeight;
    static int s_fontCharsPerRow;
    
public:
    UIButton(const std::string& name,
             const DirectX::XMFLOAT2& position,
             UIAnchor anchor = UIAnchor::CENTER,
             float width = 200.0f,
             float height = 60.0f);
    
    void Initialize() override;
    void Update(double elapsed_time) override;
    void Draw() override;
    void Finalize() override;
    
    // Set textures for different button states
    void SetTexture(int textureId) { m_textureId = textureId; }
    void SetTexture(const wchar_t* texturePath);
    void SetTexturePath(const std::string& texturePath);
    const std::string& GetTexturePath() const { return m_texturePath; }
    void SetHoverTexture(int textureId) { m_textureIdHover = textureId; }
    void SetHoverTexture(const wchar_t* texturePath);
    void SetHoverTexturePath(const std::string& texturePath);
    const std::string& GetHoverTexturePath() const { return m_hoverTexturePath; }
    void SetPressedTexture(int textureId) { m_textureIdPressed = textureId; }
    void SetPressedTexture(const wchar_t* texturePath);
    void SetPressedTexturePath(const std::string& texturePath);
    const std::string& GetPressedTexturePath() const { return m_pressedTexturePath; }
    
    // Set colors for different button states
    void SetColor(const DirectX::XMFLOAT4& color) { m_color = color; }
    const DirectX::XMFLOAT4& GetColor() const { return m_color; }
    void SetHoverColor(const DirectX::XMFLOAT4& color) { m_colorHover = color; }
    const DirectX::XMFLOAT4& GetHoverColor() const { return m_colorHover; }
    void SetPressedColor(const DirectX::XMFLOAT4& color) { m_colorPressed = color; }
    const DirectX::XMFLOAT4& GetPressedColor() const { return m_colorPressed; }
    
    // Set button size
    void SetSize(float width, float height) { m_width = width; m_height = height; }
    float GetWidth() const { return m_width; }
    float GetHeight() const { return m_height; }
    
    // Set button text (std::string for UITextRenderer compatibility)
    void SetText(const std::string& text) { m_text = text; }
    const std::string& GetText() const { return m_text; }
    void SetTextColor(const DirectX::XMFLOAT4& text_color) { m_textColor = text_color; }
    const DirectX::XMFLOAT4& GetTextColor() const { return m_textColor; }
    void SetTextHoverColor(const DirectX::XMFLOAT4& color) { m_textHoverColor = color; }
    const DirectX::XMFLOAT4& GetTextHoverColor() const { return m_textHoverColor; }
    //void SetTextPressedColor(const DirectX::XMFLOAT4& color) { m_colorPressed = color; }
    void SetTextScale(float text_scale) { m_textScale = text_scale; }
    float GetTextScale() const { return m_textScale; }

    // Set click callback
    void SetOnClick(CallbackFunction callback) { m_onClick = callback; }
    void SetAction(const std::string& action, const std::string& scene = "", bool playOnLoad = false);
    const std::string& GetAction() const { return m_action; }
    const std::string& GetActionScene() const { return m_actionScene; }
    bool GetActionPlayOnLoad() const { return m_actionPlayOnLoad; }
    
    // Manual navigation setup
    void SetNextUp(UIButton* button) { m_nextUp = button; }
    void SetNextDown(UIButton* button) { m_nextDown = button; }
    void SetNextLeft(UIButton* button) { m_nextLeft = button; }
    void SetNextRight(UIButton* button) { m_nextRight = button; }
    
    UIButton* GetNextUp() const { return m_nextUp; }
    UIButton* GetNextDown() const { return m_nextDown; }
    UIButton* GetNextLeft() const { return m_nextLeft; }
    UIButton* GetNextRight() const { return m_nextRight; }
    
    // Button state
    void SetSelected(bool selected) { m_isSelected = selected; }
    bool IsSelected() const { return m_isSelected; }
    bool IsHovered() const { return m_isHovered; }
    bool IsPressed() const { return m_isPressed; }
    
    // Trigger the button action (called when activated via keyboard/controller)
    void Activate();
    
    bool GetScreenRect(UIScreenRect& out) const override;
    bool ContainsPoint(float x, float y) const;
    
    // Static font configuration for all buttons using UITextRenderer
    static void SetFont(const wchar_t* fontTexturePath, float charWidth = 8.0f, float charHeight = 16.0f, int charsPerRow = 32);
    static void SetFontPath(const std::string& fontTexturePath, float charWidth = 8.0f, float charHeight = 16.0f, int charsPerRow = 32);
    static const std::string& GetFontPath() { return s_fontPath; }
    static float GetFontCharWidth() { return s_fontCharWidth; }
    static float GetFontCharHeight() { return s_fontCharHeight; }
    static int GetFontCharsPerRow() { return s_fontCharsPerRow; }
    static void ShutdownSharedFont();
};

#endif // UI_BUTTON_H
