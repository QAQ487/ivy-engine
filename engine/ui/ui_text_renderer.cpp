// ui_text_renderer.cpp
// Renders UI text glyphs via batched sprites.

#include "ui_text_renderer.h"
#include "ui_font.h"
#include "../rendering/sprite/sprite.h"
#include "../rendering/sprite/texture.h"
#include <algorithm>

namespace {
	constexpr float kLegacyCellPx = 32.0f;
}

UITextRenderer::UITextRenderer()
	: m_fontTextureId(-1)
	, m_characterWidth(16.0f)
	, m_characterHeight(32.0f)
	, m_characterSpacing(16.0f)
	, m_charactersPerRow(16)
	, m_isTtf(false)
	, m_ttfFont(nullptr)
{
}

float UITextRenderer::DrawScale(float scale) const {
	if (!m_isTtf || !m_ttfFont || m_ttfFont->bakeSize <= 0.0f)
		return scale;
	return scale * (kLegacyCellPx / m_ttfFont->bakeSize);
}

const UIGlyph* UITextRenderer::GlyphFor(unsigned char c) const {
	if (!m_ttfFont || c < 32 || c > 126)
		return nullptr;
	return &m_ttfFont->glyphs[c - 32];
}

void UITextRenderer::Initialize(const wchar_t* fontTexturePath,
	float charWidth,
	float charHeight,
	int charsPerRow) {
	m_characterWidth = charWidth;
	m_characterHeight = charHeight;
	m_characterSpacing = charWidth;
	m_charactersPerRow = charsPerRow;
	m_ttfFont = nullptr;
	m_isTtf = false;
	m_fontTextureId = -1;

	if (!fontTexturePath || !fontTexturePath[0])
		return;

	if (UIFont_IsTrueTypePath(fontTexturePath)) {
		m_isTtf = true;
		m_ttfFont = UIFont_Get(fontTexturePath);
		if (m_ttfFont)
			m_fontTextureId = m_ttfFont->textureId;
		return;
	}

	m_fontTextureId = TextureLoad(fontTexturePath, false);
}

void UITextRenderer::Draw(const std::string& text,
	float x, float y,
	float scale, const DirectX::XMFLOAT4& color) {
	if (m_fontTextureId < 0) return;

	if (m_isTtf && m_ttfFont) {
		const float drawScale = DrawScale(scale);
		const float lineHeight = m_ttfFont->lineHeight * drawScale;
		float currentX = x;
		float baseline = y + m_ttfFont->ascent * drawScale;

		for (char ch : text) {
			if (ch == '\n' || ch == '\r') {
				if (ch == '\n') {
					currentX = x;
					baseline += lineHeight;
				}
				continue;
			}

			const UIGlyph* glyph = GlyphFor(static_cast<unsigned char>(ch));
			if (!glyph) {
				if (ch == ' ')
					currentX += m_ttfFont->glyphs[0].xadvance * drawScale;
				continue;
			}

			if (glyph->srcW > 0 && glyph->srcH > 0) {
				Sprite_Draw(m_fontTextureId,
					currentX + glyph->xoff * drawScale,
					baseline + glyph->yoff * drawScale,
					static_cast<float>(glyph->srcW) * drawScale,
					static_cast<float>(glyph->srcH) * drawScale,
					glyph->srcX, glyph->srcY, glyph->srcW, glyph->srcH,
					color);
			}
			currentX += glyph->xadvance * drawScale;
		}
		return;
	}

	const float lineHeight = m_characterHeight * scale;
	float currentX = x;
	float currentY = y;

	for (char c : text) {
		if (c == '\n' || c == '\r') {
			if (c == '\n') {
				currentX = x;
				currentY += lineHeight;
			}
			continue;
		}

		const int charIndex = static_cast<unsigned char>(c);
		if (charIndex < 32 || charIndex > 126) {
			if (c == ' ')
				currentX += m_characterSpacing * scale;
			continue;
		}

		const int texIndex = charIndex - 32;
		const int texX = (texIndex % m_charactersPerRow) * static_cast<int>(m_characterWidth);
		const int texY = (texIndex / m_charactersPerRow) * static_cast<int>(m_characterHeight);

		Sprite_Draw(m_fontTextureId,
			currentX, currentY,
			m_characterWidth * scale, m_characterHeight * scale,
			texX, texY,
			static_cast<int>(m_characterWidth),
			static_cast<int>(m_characterHeight),
			color);

		currentX += m_characterSpacing * scale;
	}
}

float UITextRenderer::GetTextWidth(const std::string& text, float scale) const {
	if (m_isTtf && m_ttfFont) {
		const float drawScale = DrawScale(scale);
		float maxWidth = 0.0f;
		float lineWidth = 0.0f;
		for (char ch : text) {
			if (ch == '\n' || ch == '\r') {
				if (ch == '\n') {
					maxWidth = (std::max)(maxWidth, lineWidth);
					lineWidth = 0.0f;
				}
				continue;
			}
			const UIGlyph* glyph = GlyphFor(static_cast<unsigned char>(ch));
			if (glyph)
				lineWidth += glyph->xadvance * drawScale;
			else if (ch == ' ')
				lineWidth += m_ttfFont->glyphs[0].xadvance * drawScale;
		}
		return (std::max)(maxWidth, lineWidth);
	}

	float maxWidth = 0.0f;
	float lineWidth = 0.0f;

	for (char c : text) {
		if (c == '\n' || c == '\r') {
			if (c == '\n') {
				maxWidth = (std::max)(maxWidth, lineWidth);
				lineWidth = 0.0f;
			}
			continue;
		}
		if (c == ' ' || (static_cast<unsigned char>(c) >= 32 &&
			static_cast<unsigned char>(c) <= 126)) {
			lineWidth += m_characterSpacing * scale;
		}
	}

	return (std::max)(maxWidth, lineWidth);
}

float UITextRenderer::GetLineHeight(float scale) const {
	if (m_isTtf && m_ttfFont)
		return m_ttfFont->lineHeight * DrawScale(scale);
	return m_characterHeight * scale;
}

void UITextRenderer::Finalize() {
	m_fontTextureId = -1;
	m_ttfFont = nullptr;
	m_isTtf = false;
}
