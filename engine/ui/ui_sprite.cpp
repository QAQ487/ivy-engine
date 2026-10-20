#include <cmath>
#include <algorithm>
/**===========================================================================
 File   :  [ui_sprite.cpp]
 Author : Terry Yeung
 Date   : 2025/11/02
 Brief  : Implementation of sprite UI element
 ===========================================================================**/

#include "ui_sprite.h"
#include "../rendering/sprite/sprite.h"
#include "../rendering/sprite/texture.h"
#include "../ecs/string_utils.h"

UISprite::UISprite(const std::string& name,
	const DirectX::XMFLOAT2& position,
	UIAnchor anchor,
	float width,
	float height)
	: UIElement(name, position, anchor)
	, m_textureId(-1)
	, m_width(width)
	, m_height(height)
	, m_color{ 1.0f, 1.0f, 1.0f, 1.0f }
	, m_rotation(0.0f)
	, m_useSourceRect(false)
	, m_srcX(0)
	, m_srcY(0)
	, m_srcW(0)
	, m_srcH(0)
	, m_fillMethod(UISpriteFillMethod::NONE)
	, m_fillOrigin(UISpriteFillOrigin::TOP)
	, m_fillAmount(1.0f)
	, m_clockwise(true)
{
}

void UISprite::Initialize() {
	// Texture should be set via SetTexture
}

void UISprite::Update(double elapsed_time) {
	UpdateAttachedScript(elapsed_time);
}

void UISprite::Draw() {
	if (!m_isVisible) return;

	DirectX::XMFLOAT2 screenPos = GetDrawPosition();
	const DirectX::XMFLOAT2 layoutScale = UIGetLayoutScale();
	const float drawWidth = m_width * layoutScale.x;
	const float drawHeight = m_height * layoutScale.y;
	int texId = (m_textureId >= 0) ? m_textureId : TextureGetWhiteTexture();
	if (texId < 0) return;

	// Get texture size for proper texture coordinate mapping
	DirectX::XMUINT2 texSize = TextureGetSize(texId);
	int srcX = 0;
	int srcY = 0;
	int srcW = (int)texSize.x;
	int srcH = (int)texSize.y;
	if (m_useSourceRect && m_srcW > 0 && m_srcH > 0) {
		srcX = m_srcX;
		srcY = m_srcY;
		srcW = m_srcW;
		srcH = m_srcH;
	}

	// Check if we should use radial fill
	if (m_fillMethod == UISpriteFillMethod::RADIAL_360) {
		// Calculate start angle based on fill origin
		float startAngle = 0.0f;
		switch (m_fillOrigin) {
		case UISpriteFillOrigin::TOP:
			startAngle = -90.0f;  // Start from top (12 o'clock)
			break;
		case UISpriteFillOrigin::RIGHT:
			startAngle = 0.0f;    // Start from right (3 o'clock)
			break;
		case UISpriteFillOrigin::BOTTOM:
			startAngle = 90.0f;   // Start from bottom (6 o'clock)
			break;
		case UISpriteFillOrigin::LEFT:
			startAngle = 180.0f;  // Start from left (9 o'clock)
			break;
		}

		// Draw radial fill sprite
		Sprite_DrawRadialFill(texId,
			screenPos.x, screenPos.y,
			drawWidth, drawHeight,
			srcX, srcY, srcW, srcH,
			m_rotation,
			m_fillAmount,
			startAngle,
			m_clockwise,
			m_color);
	}
	else {
		// Draw sprite at calculated position
		Sprite_Draw(texId,
			screenPos.x, screenPos.y,
			drawWidth, drawHeight,
			srcX, srcY, srcW, srcH,
			m_rotation,
			m_color);
	}
}

void UISprite::Finalize() {
	TeardownAttachedScript();
}

bool UISprite::GetScreenRect(UIScreenRect& out) const {
	if (!m_isVisible)
		return false;

	DirectX::XMFLOAT2 screenPos = GetDrawPosition();
	const DirectX::XMFLOAT2 layoutScale = UIGetLayoutScale();
	const float drawWidth = m_width * layoutScale.x;
	const float drawHeight = m_height * layoutScale.y;

	out.left = screenPos.x;
	out.top = screenPos.y;
	out.right = screenPos.x + drawWidth;
    out.bottom = screenPos.y + drawHeight;
    if(HasExplicitPivot() && m_rotation!=0) {
        const float cx=screenPos.x+drawWidth*.5f,cy=screenPos.y+drawHeight*.5f;
        const float angle=DirectX::XMConvertToRadians(m_rotation);
        const float extentX=std::fabs(std::cos(angle))*drawWidth*.5f+std::fabs(std::sin(angle))*drawHeight*.5f;
        const float extentY=std::fabs(std::sin(angle))*drawWidth*.5f+std::fabs(std::cos(angle))*drawHeight*.5f;
        out={cx-extentX,cy-extentY,cx+extentX,cy+extentY};
    }
    return true;
}

void UISprite::SetTexture(const wchar_t* texturePath) {
	m_textureId = TextureLoad(texturePath, false);
}

void UISprite::SetTexturePath(const std::string& texturePath) {
	m_texturePath = texturePath;
	if (texturePath.empty()) {
		m_textureId = -1;
		return;
	}
	std::wstring widePath = ToWString(texturePath);
	if (!widePath.empty())
		SetTexture(widePath.c_str());
}

void UISprite::SetSourceRect(int x, int y, int width, int height) {
	m_useSourceRect = true;
	m_srcX = x;
	m_srcY = y;
	m_srcW = width;
	m_srcH = height;
}

void UISprite::ClearSourceRect() {
	m_useSourceRect = false;
	m_srcX = 0;
	m_srcY = 0;
	m_srcW = 0;
	m_srcH = 0;
}

DirectX::XMFLOAT2 UISprite::GetDrawPosition() const {
    auto position=GetScreenPosition();
    if(HasExplicitPivot() && m_rotation!=0) {
        const auto scale=UIGetLayoutScale(),pivot=GetPivot();
        const float dx=(.5f-pivot.x)*m_width*scale.x;
        const float dy=(pivot.y-.5f)*m_height*scale.y;
        const float angle=DirectX::XMConvertToRadians(m_rotation),c=std::cos(angle),s=std::sin(angle);
        position.x+=dx*c-dy*s-dx;
        position.y+=dx*s+dy*c-dy;
    }
    return position;
}
