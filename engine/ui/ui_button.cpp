/**===========================================================================
 File   :  [ui_button.cpp]
 Author : Copilot Agent
 Date   : 2025/11/17
 Brief  : Implementation of interactive UI button
 ===========================================================================**/

#include "ui_button.h"
#include "../rendering/sprite/sprite.h"
#include "../rendering/sprite/texture.h"
#include "../input/mouse.h"
#include "../ecs/string_utils.h"
#include "../core/engine_gui.h"

// Static text renderer for all buttons
UITextRenderer UIButton::s_textRenderer;
bool UIButton::s_textRendererInitialized = false;
std::string UIButton::s_fontPath;
float UIButton::s_fontCharWidth = 8.0f;
float UIButton::s_fontCharHeight = 16.0f;
int UIButton::s_fontCharsPerRow = 32;

UIButton::UIButton(const std::string& name,
	const DirectX::XMFLOAT2& position,
	UIAnchor anchor,
	float width,
	float height)
	: UIElement(name, position, anchor)
	, m_textureId(-1)
	, m_textureIdHover(-1)
	, m_textureIdPressed(-1)
	, m_width(width)
	, m_height(height)
	, m_color{ 1.0f, 1.0f, 1.0f, 0.7f }       // 70% opacity in normal state
	, m_colorHover{ 1.0f, 1.0f, 1.0f, 1.0f }  // Full opacity when hovered
	, m_colorPressed{ 0.5f, 0.5f, 0.5f, 1.0f } // Darker gray when pressed
	, m_textColor({ 1.0f,1.0f,1.0f,1.0f })
	, m_textHoverColor({ 1.0f,1.0f,1.0f,1.0f })
	, m_textScale(1.0f)
	, m_isHovered(false)
	, m_isPressed(false)
	, m_isSelected(false)
	, m_wasMouseDown(false)
	, m_onClick(nullptr)
	, m_actionPlayOnLoad(false)
	, m_nextUp(nullptr)
	, m_nextDown(nullptr)
	, m_nextLeft(nullptr)
	, m_nextRight(nullptr)
{
}

void UIButton::Initialize() {
	// Button can be initialized with textures via SetTexture methods
}

void UIButton::Update(double elapsed_time) {
	UpdateAttachedScript(elapsed_time);
	if (!m_isVisible) return;

	int mouseX = 0;
	int mouseY = 0;
	Mouse_GetPosition(mouseX, mouseY);

	float viewportX = static_cast<float>(mouseX);
	float viewportY = static_cast<float>(mouseY);
	if (!EngineGUI_IsPlaying()) {
		float vx = 0.0f, vy = 0.0f, vw = 0.0f, vh = 0.0f;
		EngineGUI_GetGameViewportRect(vx, vy, vw, vh);
		viewportX -= vx;
		viewportY -= vy;
	}
	m_isHovered = ContainsPoint(viewportX, viewportY);

	const bool mouseDown = Mouse_IsLeftDown();
	const bool mouseReleased = !mouseDown && m_wasMouseDown;
	const bool mouseClicked = m_isHovered && mouseReleased;
	m_isPressed = m_isHovered && mouseDown;
	m_wasMouseDown = mouseDown;

	if (mouseClicked) {
		Activate();
	}
}

void UIButton::Draw() {
	if (!m_isVisible) return;

	DirectX::XMFLOAT2 screenPos = GetScreenPosition();
	const DirectX::XMFLOAT2 layoutScale = UIGetLayoutScale();
	const float drawWidth = m_width * layoutScale.x;
	const float drawHeight = m_height * layoutScale.y;

	// Determine which texture and color to use based on state
	int texId = m_textureId;
	DirectX::XMFLOAT4 color = m_color;
	DirectX::XMFLOAT4 text_color = m_textColor;

	if (m_isPressed && m_textureIdPressed >= 0) {
		texId = m_textureIdPressed;
		color = m_colorPressed;
		text_color = m_textColor;
	}
	else if (m_isHovered && m_textureIdHover >= 0) {
		texId = m_textureIdHover;
		color = m_colorHover;
		text_color = m_textHoverColor;
	}
	else if (m_isHovered) {
		color = m_colorHover;
		text_color = m_textHoverColor;
	}

	if (texId < 0)
		texId = TextureGetWhiteTexture();

	if (texId >= 0) {
		DirectX::XMUINT2 texSize = TextureGetSize(texId);
		Sprite_Draw(texId,
			screenPos.x, screenPos.y,
			drawWidth, drawHeight,
			0, 0, (int)texSize.x, (int)texSize.y,
			0.0f,
			color);
	}

	// Draw text if set and text renderer is initialized
	if (!m_text.empty() && s_textRendererInitialized) {
		const float textScale = m_textScale * layoutScale.x;
		// Calculate text dimensions using UITextRenderer with scale
		float textWidth = s_textRenderer.GetTextWidth(m_text, textScale);
		float textHeight = s_textRenderer.GetCharacterSpacing() * textScale;

		// Center text on button (accounting for scale)
		float textX = screenPos.x + drawWidth * 0.5f - textWidth * 0.5f;
		float textY = screenPos.y + drawHeight * 0.5f - textHeight * 0.5f;

		// Draw text using UITextRenderer with scale and color
		s_textRenderer.Draw(m_text, textX, textY, textScale, text_color);
	}
}

void UIButton::Finalize() {
	TeardownAttachedScript();
	m_nextUp = nullptr;
	m_nextDown = nullptr;
	m_nextLeft = nullptr;
	m_nextRight = nullptr;
}

void UIButton::SetTexture(const wchar_t* texturePath) {
	m_textureId = TextureLoad(texturePath, false);
}

void UIButton::SetTexturePath(const std::string& texturePath) {
	m_texturePath = texturePath;
	if (texturePath.empty()) {
		m_textureId = -1;
		return;
	}
	std::wstring widePath = ToWString(texturePath);
	if (!widePath.empty())
		SetTexture(widePath.c_str());
}

void UIButton::SetHoverTexture(const wchar_t* texturePath) {
	m_textureIdHover = TextureLoad(texturePath, false);
}

void UIButton::SetHoverTexturePath(const std::string& texturePath) {
	m_hoverTexturePath = texturePath;
	if (texturePath.empty()) {
		m_textureIdHover = -1;
		return;
	}
	std::wstring widePath = ToWString(texturePath);
	if (!widePath.empty())
		SetHoverTexture(widePath.c_str());
}

void UIButton::SetPressedTexture(const wchar_t* texturePath) {
	m_textureIdPressed = TextureLoad(texturePath, false);
}

void UIButton::SetPressedTexturePath(const std::string& texturePath) {
	m_pressedTexturePath = texturePath;
	if (texturePath.empty()) {
		m_textureIdPressed = -1;
		return;
	}
	std::wstring widePath = ToWString(texturePath);
	if (!widePath.empty())
		SetPressedTexture(widePath.c_str());
}

void UIButton::SetAction(const std::string& action, const std::string& scene, bool playOnLoad) {
	m_action = action;
	m_actionScene = scene;
	m_actionPlayOnLoad = playOnLoad;
}

void UIButton::Activate() {
	if (!m_isVisible) return;
	if (!EngineGUI_IsPlaying()) return;

	m_isPressed = true;

	// Builtin / registered action callback (LoadScene, ExitGame, custom binders)
	if (m_onClick) {
		m_onClick();
	}

	// Optional UICanvasScript OnClick for content-authored click logic
	InvokeAttachedScriptOnClick();

	// Reset pressed state after a short time (handled in next update)
	m_isPressed = false;
}

bool UIButton::GetScreenRect(UIScreenRect& out) const {
	if (!m_isVisible)
		return false;

	DirectX::XMFLOAT2 screenPos = GetScreenPosition();
	const DirectX::XMFLOAT2 layoutScale = UIGetLayoutScale();
	const float drawWidth = m_width * layoutScale.x;
	const float drawHeight = m_height * layoutScale.y;

	out.left = screenPos.x;
	out.top = screenPos.y;
	out.right = screenPos.x + drawWidth;
	out.bottom = screenPos.y + drawHeight;
	return true;
}

bool UIButton::ContainsPoint(float x, float y) const {
	return HitTest(x, y);
}

void UIButton::SetFont(const wchar_t* fontTexturePath, float charWidth, float charHeight, int charsPerRow) {
	s_textRenderer.Initialize(fontTexturePath, charWidth, charHeight, charsPerRow);
	s_textRendererInitialized = true;
	s_fontCharWidth = charWidth;
	s_fontCharHeight = charHeight;
	s_fontCharsPerRow = charsPerRow;
}

void UIButton::SetFontPath(const std::string& fontTexturePath, float charWidth, float charHeight, int charsPerRow) {
	s_fontPath = fontTexturePath;
	std::wstring widePath = ToWString(fontTexturePath);
	if (!widePath.empty())
		SetFont(widePath.c_str(), charWidth, charHeight, charsPerRow);
}

void UIButton::ShutdownSharedFont() {
	s_textRenderer.Finalize();
	s_textRendererInitialized = false;
	s_fontPath.clear();
}
