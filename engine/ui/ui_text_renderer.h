/**===========================================================================
 File   :  [ui_text_renderer.h]
 Author : Copilot Agent
 Date   : 2025/11/02
 Brief  : Simple text renderer for UI using sprite-based characters
 ===========================================================================**/

#ifndef UI_TEXT_RENDERER_H
#define UI_TEXT_RENDERER_H

#include <DirectXMath.h>
#include <string>

 /**
  * Simple text renderer for UI elements.
  * Renders text using individual sprites for each character.
  * Allows full control over spacing and positioning.
  */
class UITextRenderer {
private:
    int m_fontTextureId;
    float m_characterWidth;
    float m_characterHeight;
    float m_characterSpacing;
    int m_charactersPerRow;

public:
    UITextRenderer();

    // Initialize with font texture
    void Initialize(const wchar_t* fontTexturePath,
        float charWidth = 16.0f,
        float charHeight = 32.0f,
        int charsPerRow = 16);

    // Draw text at specified position
    void Draw(const std::string& text,
        float x, float y,
        float scale = 1.0f, const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

    // Set character spacing (default: same as character width)
    void SetCharacterSpacing(float spacing) { m_characterSpacing = spacing; }
    float GetCharacterSpacing() const { return m_characterSpacing; }

    // Get text width for positioning (with optional scale)
    float GetTextWidth(const std::string& text, float scale = 1.0f) const;

    // Check if the text renderer is properly initialized with a valid font
    bool IsInitialized() const { return m_fontTextureId >= 0; }

    void Finalize();
};

#endif // UI_TEXT_RENDERER_H