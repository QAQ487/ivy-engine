// ui_text_renderer.h
// Renders UI text glyphs via batched sprites.

#ifndef UI_TEXT_RENDERER_H
#define UI_TEXT_RENDERER_H

#include <DirectXMath.h>
#include <string>

class UITextRenderer {
private:
    int m_fontTextureId;
    float m_characterWidth;
    float m_characterHeight;
    float m_characterSpacing;
    int m_charactersPerRow;
    bool m_isTtf;
    const struct UIFont* m_ttfFont;

    float DrawScale(float scale) const;
    const struct UIGlyph* GlyphFor(unsigned char c) const;

public:
    UITextRenderer();

    void Initialize(const wchar_t* fontTexturePath,
        float charWidth = 16.0f,
        float charHeight = 32.0f,
        int charsPerRow = 16);

    void Draw(const std::string& text,
        float x, float y,
        float scale = 1.0f, const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

    void SetCharacterSpacing(float spacing) { m_characterSpacing = spacing; }
    float GetCharacterSpacing() const { return m_characterSpacing; }

    float GetTextWidth(const std::string& text, float scale = 1.0f) const;
    float GetLineHeight(float scale = 1.0f) const;

    bool IsInitialized() const { return m_fontTextureId >= 0; }

    void Finalize();
};

#endif
