/**===========================================================================
 File   :  [ui_text.cpp]
 Author : Terry Yeung
 Date   : 2025/11/22
 Brief  : Implementation of UI text element
 ===========================================================================**/

#include "ui_text.h"
#include "../ecs/string_utils.h"

UIText::UIText(const std::string& name,
	const DirectX::XMFLOAT2& position,
	UIAnchor anchor,
	const std::string& text)
	: UIElement(name, position, anchor)
	, m_text(text)
	, m_color(1.0f, 1.0f, 1.0f, 1.0f)  // White by default
	, m_fontSize(1.0f)
	, m_characterSpacing(32.0f)
	, m_alignment(TextAlign::LEFT)
	, m_fontCharWidth(32.0f)
	, m_fontCharHeight(32.0f)
	, m_fontCharsPerRow(16)
	, m_rendererInitialized(false)
{
}

void UIText::Initialize() {
	// Note: SetFont() must be called before the text can be rendered.
	// This allows flexibility in choosing font textures and character dimensions.
	// If SetFont() is not called, the text will not be displayed.
}

void UIText::Update(double elapsed_time) {
	UpdateAttachedScript(elapsed_time);
}

void UIText::Draw() {
	if (!m_isVisible || m_text.empty() || !m_rendererInitialized) {
		return;
	}

	// Get screen position based on anchor
	DirectX::XMFLOAT2 screenPos = GetScreenPosition();
	const DirectX::XMFLOAT2 layoutScale = UIGetLayoutScale();
	const float drawFontSize = m_fontSize * layoutScale.x;

	m_textRenderer.SetCharacterSpacing(HasExplicitPivot()?m_characterSpacing:m_characterSpacing * layoutScale.x);

	float alignmentOffset = 0.0f;
	const float textWidth = HasExplicitPivot()?GetPivotDesignSize().x*layoutScale.x:m_textRenderer.GetTextWidth(m_text, drawFontSize);
	switch (HasExplicitPivot() ? TextAlign::LEFT : m_alignment) {
	case TextAlign::LEFT:
		alignmentOffset = 0.0f;
		break;
	case TextAlign::CENTER:
		alignmentOffset = -textWidth / 2.0f;
		break;
	case TextAlign::RIGHT:
		alignmentOffset = -textWidth;
		break;
	}

	// Draw text with alignment offset
	m_textRenderer.Draw(m_text,
		screenPos.x + alignmentOffset,
		screenPos.y,
		drawFontSize,
		m_color);
}

void UIText::Finalize() {
	TeardownAttachedScript();
	if (m_rendererInitialized) {
		m_textRenderer.Finalize();
		m_rendererInitialized = false;
	}
}

void UIText::SetFont(const wchar_t* fontTexturePath,
	float charWidth,
	float charHeight,
	int charsPerRow) {
	// Finalize previous font if any
	if (m_rendererInitialized) {
		m_textRenderer.Finalize();
		m_rendererInitialized = false;
	}

	// Initialize with new font
	m_textRenderer.Initialize(fontTexturePath, charWidth, charHeight, charsPerRow);

	m_rendererInitialized = m_textRenderer.IsInitialized();
	m_fontCharWidth = charWidth;
	m_fontCharHeight = charHeight;
	m_fontCharsPerRow = charsPerRow;
	if (m_rendererInitialized)
		m_textRenderer.SetCharacterSpacing(m_characterSpacing);
}

void UIText::SetFontPath(const std::string& fontTexturePath,
	float charWidth,
	float charHeight,
	int charsPerRow) {
	m_fontPath = fontTexturePath;
	std::wstring widePath = ToWString(fontTexturePath);
	if (!widePath.empty())
		SetFont(widePath.c_str(), charWidth, charHeight, charsPerRow);
}

float UIText::GetTextWidth() const {
	if (!m_rendererInitialized) {
		return 0.0f;
	}
	auto measurement=m_textRenderer;
    measurement.SetCharacterSpacing(m_characterSpacing);
    return measurement.GetTextWidth(m_text, m_fontSize);
}

bool UIText::GetScreenRect(UIScreenRect& out) const {
	if (!m_isVisible || m_text.empty() || !m_rendererInitialized)
		return false;

	DirectX::XMFLOAT2 screenPos = GetScreenPosition();
	const DirectX::XMFLOAT2 layoutScale = UIGetLayoutScale();
	const float drawFontSize = m_fontSize * layoutScale.x;
	const float textWidth = HasExplicitPivot()?GetPivotDesignSize().x*layoutScale.x:m_textRenderer.GetTextWidth(m_text, drawFontSize);
	const float textHeight = HasExplicitPivot()?GetPivotDesignSize().y*layoutScale.x:m_textRenderer.GetLineHeight(drawFontSize);

	float alignmentOffset = 0.0f;
	switch (HasExplicitPivot() ? TextAlign::LEFT : m_alignment) {
	case TextAlign::LEFT:
		alignmentOffset = 0.0f;
		break;
	case TextAlign::CENTER:
		alignmentOffset = -textWidth / 2.0f;
		break;
	case TextAlign::RIGHT:
		alignmentOffset = -textWidth;
		break;
	}

	const float left = screenPos.x + alignmentOffset;
	out.left = left;
	out.top = screenPos.y;
	out.right = left + textWidth;
	out.bottom = screenPos.y + textHeight;
	return true;
}

DirectX::XMFLOAT2 UIText::GetPivotDesignSize() const {
    if(!m_rendererInitialized) return {};
    std::size_t lines=1;
    for(char c:m_text) if(c=='\n') ++lines;
    return {GetTextWidth(),m_textRenderer.GetLineHeight(m_fontSize)*float(lines)};
}
