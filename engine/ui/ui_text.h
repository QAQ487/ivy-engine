/**===========================================================================
 File   :  [ui_text.h]
 Author : Terry Yeung
 Date   : 2025/11/22
 Brief  : UI text element with alignment, font, size, and color control
 ===========================================================================**/

#ifndef UI_TEXT_H
#define UI_TEXT_H

#include "ui_element.h"
#include "ui_text_renderer.h"
#include <DirectXMath.h>
#include <string>

/**
 * Text alignment options for UIText
 */
enum class TextAlign {
    LEFT,
    CENTER,
    RIGHT
};

/**
 * UI element that displays text with customizable alignment, font, size, and color.
 * Position is relative to the anchor point.
 * Text alignment determines how the text is aligned relative to the position.
 * 
 * IMPORTANT: You must call SetFont() before the text can be displayed!
 * The text will not render until a font texture has been configured.
 */
class UIText : public UIElement {
private:
    std::string m_text;
    DirectX::XMFLOAT4 m_color;
    float m_fontSize;  // Scale factor for text size
    float m_characterSpacing;
    TextAlign m_alignment;
    std::string m_fontPath;
    float m_fontCharWidth;
    float m_fontCharHeight;
    int m_fontCharsPerRow;
    UITextRenderer m_textRenderer;
    bool m_rendererInitialized;
    
public:
    UIText(const std::string& name,
           const DirectX::XMFLOAT2& position,
           UIAnchor anchor = UIAnchor::CENTER,
           const std::string& text = "");
    
    void Initialize() override;
    void Update(double elapsed_time) override;
    void Draw() override;
    void Finalize() override;

    bool GetScreenRect(UIScreenRect& out) const override;
    DirectX::XMFLOAT2 GetPivotDesignSize() const;
    
    // Set text content
    void SetText(const std::string& text) { m_text = text; }
    const std::string& GetText() const { return m_text; }
    
    // Set text color
    void SetColor(const DirectX::XMFLOAT4& color) { m_color = color; }
    const DirectX::XMFLOAT4& GetColor() const { return m_color; }
    
    // Set font size (scale factor)
    void SetFontSize(float size) { m_fontSize = size; }
    float GetFontSize() const { return m_fontSize; }

    // set character spacing
    void SetCharacterSpacing(float character_spacing) {
        m_characterSpacing = character_spacing;
        if (m_rendererInitialized)
            m_textRenderer.SetCharacterSpacing(m_characterSpacing);
    }
    float GetCharacterSpacing() const { return m_characterSpacing; }
    
    // Set text alignment
    void SetAlignment(TextAlign alignment) { m_alignment = alignment; }
    TextAlign GetAlignment() const { return m_alignment; }
    
    // Set font texture and properties (REQUIRED before text can be displayed)
    // If the font texture fails to load, text may not render correctly.
    // Use a valid font texture path with the correct character dimensions.
    void SetFont(const wchar_t* fontTexturePath,
                 float charWidth = 16.0f,
                 float charHeight = 32.0f,
                 int charsPerRow = 16);
    void SetFontPath(const std::string& fontTexturePath,
                     float charWidth = 16.0f,
                     float charHeight = 32.0f,
                     int charsPerRow = 16);
    const std::string& GetFontPath() const { return m_fontPath; }
    float GetFontCharWidth() const { return m_fontCharWidth; }
    float GetFontCharHeight() const { return m_fontCharHeight; }
    int GetFontCharsPerRow() const { return m_fontCharsPerRow; }
    
    // Get text width for positioning calculations
    float GetTextWidth() const;
};

#endif // UI_TEXT_H
