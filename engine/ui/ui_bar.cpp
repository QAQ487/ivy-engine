/**===========================================================================
 File   :  [ui_bar.cpp]
 Author : Terry Yeung
 Date   : 2025/11/17
 Brief  : Implementation of universal UI progress bar element
 ===========================================================================**/

#include "ui_bar.h"
#include "../rendering/sprite/sprite.h"
#include "../rendering/sprite/texture.h"
#include "../ecs/string_utils.h"
#include <sstream>
#include <iomanip>

UIBar::UIBar(const std::string& name,
	const DirectX::XMFLOAT2& position,
	UIAnchor anchor,
	float maxValue,
	const std::string& label)
	: UIElement(name, position, anchor)
	, m_barBgTexture(-1)
	, m_barFillTexture(-1)
	, m_currentValue(0.0f)
	, m_maxValue(maxValue)
	, m_label(label)
	, m_textColor{ 1.0f, 1.0f, 1.0f, 1.0f }
	, m_barColor{ 1.0f, 1.0f, 1.0f, 1.0f }
	, m_backgroundColor{ 1.0f,1.0f,1.0f,1.0f }
	, m_barWidth(400)
	, m_barHeight(25)
	, m_characterSpacing(15.0f)
	, m_showText(false)
	, m_showValue(true)
{

}

void UIBar::Initialize() {
	// Create text renderer
	m_textRenderer = std::make_unique<UITextRenderer>();
	m_textRenderer->Initialize(
		L"builtin:font/IBMPlexMono-Regular.ttf",
		32.0f,
		32.0f,
		16
	);
	m_textRenderer->SetCharacterSpacing(m_characterSpacing);
}

void UIBar::Update(double elapsed_time) {
	UpdateAttachedScript(elapsed_time);
	if (m_onUpdate) {
		m_onUpdate(this);
	}
}

void UIBar::Draw() {
	if (!m_isVisible) return;

	// Get screen position based on anchor
	DirectX::XMFLOAT2 screenPos = GetScreenPosition();
	const DirectX::XMFLOAT2 layoutScale = UIGetLayoutScale();
	const float drawBarWidth = static_cast<float>(m_barWidth) * layoutScale.x;
	const float drawBarHeight = static_cast<float>(m_barHeight) * layoutScale.y;

	float barX = screenPos.x;
	float barY = screenPos.y;

	// Draw label text above the bar
	if (m_showText && m_textRenderer) {
		std::ostringstream oss;

		if (!m_label.empty()) {
			oss << m_label;
			if (m_showValue) {
				oss << ": ";
			}
		}

		if (m_showValue) {
			// Check if values are integers or floats
			if (m_maxValue == static_cast<int>(m_maxValue) &&
				m_currentValue == static_cast<int>(m_currentValue)) {
				// Display as integers
				oss << static_cast<int>(m_currentValue) << "/" << static_cast<int>(m_maxValue);
			}
			else {
				// Display as floats with 1 decimal place
				oss << std::fixed << std::setprecision(1)
					<< m_currentValue << "/" << m_maxValue;
			}
		}

		// Draw text above the bar
		m_textRenderer->Draw(oss.str(), barX, barY - 35.0f * layoutScale.y, layoutScale.x, m_textColor);
	}

	// Calculate progress (0.0 to 1.0)
	float progress = 0.0f;
	if (m_maxValue > 0.0f) {
		progress = m_currentValue / m_maxValue;
		progress = (progress < 0.0f) ? 0.0f : (progress > 1.0f ? 1.0f : progress);
	}

	// Draw background bar
	int bgTexture = (m_barBgTexture >= 0) ? m_barBgTexture : TextureGetWhiteTexture();
	int fillTexture = (m_barFillTexture >= 0) ? m_barFillTexture : TextureGetWhiteTexture();
	if (bgTexture < 0 || fillTexture < 0) return;

	Sprite_Draw(bgTexture, barX, barY,
		drawBarWidth,
		drawBarHeight,
		0, 0, TextureGetWidth(bgTexture), TextureGetHeight(bgTexture), m_backgroundColor);

	// Draw foreground bar (progress)
	float fillWidth = drawBarWidth * progress;
	if (fillWidth > 0.0f) {
		Sprite_Draw(fillTexture, barX, barY, fillWidth,
			drawBarHeight,
			0, 0, static_cast<int>(progress * TextureGetWidth(fillTexture)), TextureGetHeight(fillTexture), m_barColor);
	}
}

bool UIBar::GetScreenRect(UIScreenRect& out) const {
	if (!m_isVisible)
		return false;

	DirectX::XMFLOAT2 screenPos = GetScreenPosition();
	const DirectX::XMFLOAT2 layoutScale = UIGetLayoutScale();
	const float drawBarWidth = static_cast<float>(m_barWidth) * layoutScale.x;
	const float drawBarHeight = static_cast<float>(m_barHeight) * layoutScale.y;

	out.left = screenPos.x;
	out.top = screenPos.y;
	out.right = screenPos.x + drawBarWidth;
	out.bottom = screenPos.y + drawBarHeight;

	if (m_showText) {
		const float labelTop = screenPos.y - 35.0f * layoutScale.y;
		if (labelTop < out.top)
			out.top = labelTop;
	}

	return true;
}

void UIBar::Finalize() {
	TeardownAttachedScript();
	if (m_textRenderer) {
		m_textRenderer->Finalize();
		m_textRenderer.reset();
	}
}

void UIBar::SetBackgroundTexturePath(const std::string& texturePath) {
	m_backgroundTexturePath = texturePath;
	if (texturePath.empty()) {
		m_barBgTexture = -1;
		return;
	}
	const std::wstring widePath = ToWString(texturePath);
	if (!widePath.empty())
		m_barBgTexture = TextureLoad(widePath.c_str(), false);
}

void UIBar::SetFillTexturePath(const std::string& texturePath) {
	m_fillTexturePath = texturePath;
	if (texturePath.empty()) {
		m_barFillTexture = -1;
		return;
	}
	const std::wstring widePath = ToWString(texturePath);
	if (!widePath.empty())
		m_barFillTexture = TextureLoad(widePath.c_str(), false);
}
