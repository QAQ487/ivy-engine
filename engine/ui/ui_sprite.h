/**===========================================================================
 File   :  [ui_sprite.h]
 Author : Terry Yeung
 Date   : 2025/11/02
 Brief  : UI element for displaying a sprite at a custom position
 ===========================================================================**/

#ifndef UI_SPRITE_H
#define UI_SPRITE_H

#include "ui_element.h"
#include <DirectXMath.h>
#include <string>

/**
 * Fill method for sprite rendering
 */
enum class UISpriteFillMethod {
    NONE,        // No fill, render entire sprite
    RADIAL_360   // Radial fill 360 degrees (like Unity's Radial360)
};

/**
 * Fill origin for radial fill
 */
enum class UISpriteFillOrigin {
    TOP,
    RIGHT,
    BOTTOM,
    LEFT
};

/**
 * UI element that displays a sprite at a custom position.
 * Position is relative to the anchor point.
 * Supports radial fill for circular progress indicators.
 */
class UISprite : public UIElement {
private:
    int m_textureId;
    std::string m_texturePath;
    float m_width;
    float m_height;
    DirectX::XMFLOAT4 m_color;
    float m_rotation;  // Rotation in degrees

    // Optional atlas source rect in texture pixels (full texture when unused)
    bool m_useSourceRect;
    int m_srcX;
    int m_srcY;
    int m_srcW;
    int m_srcH;
    
    // Fill properties
    UISpriteFillMethod m_fillMethod;
    UISpriteFillOrigin m_fillOrigin;
    float m_fillAmount;  // 0.0 to 1.0
    bool m_clockwise;    // true for clockwise, false for counter-clockwise
    
public:
    UISprite(const std::string& name, 
             const DirectX::XMFLOAT2& position,
             UIAnchor anchor = UIAnchor::CENTER,
             float width = 64.0f,
             float height = 64.0f);
    
    void Initialize() override;
    void Update(double elapsed_time) override;
    void Draw() override;
    void Finalize() override;

    bool GetScreenRect(UIScreenRect& out) const override;
    DirectX::XMFLOAT2 GetDrawPosition() const;
    DirectX::XMFLOAT2 GetPivotDesignSize() const {return {m_width,m_height};}
    
    // Set the texture to display
    void SetTexture(int textureId) { m_textureId = textureId; }
    void SetTexture(const wchar_t* texturePath);
    void SetTexturePath(const std::string& texturePath);
    const std::string& GetTexturePath() const { return m_texturePath; }

    // Atlas cell in texture pixels; ClearSourceRect draws the full texture
    void SetSourceRect(int x, int y, int width, int height);
    void ClearSourceRect();
    bool HasSourceRect() const { return m_useSourceRect; }
    
    // Set size
    void SetSize(float width, float height) { m_width = width; m_height = height; }
    float GetWidth() const { return m_width; }
    float GetHeight() const { return m_height; }
    
    // Set color tint
    void SetColor(const DirectX::XMFLOAT4& color) { m_color = color; }
    const DirectX::XMFLOAT4& GetColor() const { return m_color; }
    
    // Set rotation
    void SetRotation(float degrees) { m_rotation = degrees; }
    float GetRotation() const { return m_rotation; }
    
    // Fill properties for circular progress
    void SetFillMethod(UISpriteFillMethod method) { m_fillMethod = method; }
    void SetFillOrigin(UISpriteFillOrigin origin) { m_fillOrigin = origin; }
    void SetFillAmount(float amount) { m_fillAmount = (amount < 0.0f) ? 0.0f : (amount > 1.0f ? 1.0f : amount); }
    void SetFillClockwise(bool clockwise) { m_clockwise = clockwise; }
    
    UISpriteFillMethod GetFillMethod() const { return m_fillMethod; }
    UISpriteFillOrigin GetFillOrigin() const { return m_fillOrigin; }
    float GetFillAmount() const { return m_fillAmount; }
    bool IsClockwise() const { return m_clockwise; }
};

#endif // UI_SPRITE_H
