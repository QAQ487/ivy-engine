/**===========================================================================
 File   :  [ui_text_renderer.cpp]
 Author : Copilot Agent
 Date   : 2025/11/02
 Brief  : Implementation of simple text renderer for UI
 ===========================================================================**/

#include "ui_text_renderer.h"
#include "../rendering/sprite/sprite.h"
#include "../rendering/sprite/texture.h"
#include <algorithm>

UITextRenderer::UITextRenderer()
	: m_fontTextureId(-1)
	, m_characterWidth(16.0f)
	, m_characterHeight(32.0f)
	, m_characterSpacing(16.0f)
	, m_charactersPerRow(16)
{
}

void UITextRenderer::Initialize(const wchar_t* fontTexturePath,
	float charWidth,
	float charHeight,
	int charsPerRow) {
	m_characterWidth = charWidth;
	m_characterHeight = charHeight;
	m_characterSpacing = charWidth; // Default spacing = character width
	m_charactersPerRow = charsPerRow;

	// Load font texture
	m_fontTextureId = TextureLoad(fontTexturePath, false);
}

void UITextRenderer::Draw(const std::string& text,
	float x, float y,
	float scale, const DirectX::XMFLOAT4& color) {
	if (m_fontTextureId < 0) return;

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

void UITextRenderer::Finalize() {
	// Texture is managed externally
	m_fontTextureId = -1;
}