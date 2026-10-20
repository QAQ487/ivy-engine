/**===========================================================================
 File   :  [ui_bar.h]
 Author : Terry Yeung
 Date   : 2025/11/17
 Brief  : Universal UI progress bar element for HP, scraps, stamina, etc.
 ===========================================================================**/

#ifndef UI_BAR_H
#define UI_BAR_H

#include <functional>

#include "ui_element.h"
#include "ui_text_renderer.h"
#include <memory>
#include <string>

/**
 * Universal UI progress bar element.
 * Can be used for HP, scraps, stamina, energy, or any value/max display.
 * Shows label text and progress bar with customizable colors.
 */
class UIBar : public UIElement {
public:
    using CallbackFunction = std::function<void(UIBar* bar)>;
private:
    float m_currentValue;
    float m_maxValue;
    std::unique_ptr<UITextRenderer> m_textRenderer;
    std::string m_label;
    DirectX::XMFLOAT4 m_textColor;
    DirectX::XMFLOAT4 m_barColor;
    DirectX::XMFLOAT4 m_backgroundColor;
    int m_barWidth;
    int m_barHeight;
    int m_barBgTexture;
    int m_barFillTexture;
    std::string m_backgroundTexturePath;
    std::string m_fillTexturePath;
    float m_characterSpacing;
    bool m_showText;
    bool m_showValue;
    CallbackFunction m_onUpdate;
    
public:
    UIBar(const std::string& name,
          const DirectX::XMFLOAT2& position,
          UIAnchor anchor = UIAnchor::TOP_LEFT,
          float maxValue = 100.0f,
          const std::string& label = "");
    
    void Initialize() override;
    void Update(double elapsed_time) override;
    void Draw() override;
    void Finalize() override;

    bool GetScreenRect(UIScreenRect& out) const override;
    DirectX::XMFLOAT2 GetPivotDesignSize() const {return {float(m_barWidth),float(m_barHeight)};}
    
    // Update bar values
    void SetCurrentValue(float value) { m_currentValue = value; }
    void SetMaxValue(float maxValue) { m_maxValue = maxValue; }
    void SetValue(float current, float max) { m_currentValue = current; m_maxValue = max; }
    
    float GetCurrentValue() const { return m_currentValue; }
    float GetMaxValue() const { return m_maxValue; }
    float GetProgress() const { return (m_maxValue > 0.0f) ? (m_currentValue / m_maxValue) : 0.0f; }
    
    // Appearance customization
    void SetLabel(const std::string& label) { m_label = label; }
    void SetBarColor(const DirectX::XMFLOAT4& color) { m_barColor = color; }
    void SetBackgroundColor(const DirectX::XMFLOAT4& color) { m_backgroundColor = color; }
    void SetTextColor(const DirectX::XMFLOAT4& color) { m_textColor = color; }
    void SetBarSize(int width, int height) { m_barWidth = width; m_barHeight = height; }
    void SetShowText(bool show) { m_showText = show; }
    void SetShowValue(bool show) { m_showValue = show; }
    void SetCharacterSpacing(float spacing) { m_characterSpacing = spacing; }
    void SetBackgroundTexture(int bg_texid) { m_barBgTexture = bg_texid; }
    void SetFillTexture(int fill_texid) { m_barFillTexture = fill_texid; }
    void SetBackgroundTexturePath(const std::string& texturePath);
    void SetFillTexturePath(const std::string& texturePath);
    const std::string& GetBackgroundTexturePath() const { return m_backgroundTexturePath; }
    const std::string& GetFillTexturePath() const { return m_fillTexturePath; }

    // Set click callback
    void SetOnUpdate(CallbackFunction onUpdate) { m_onUpdate = onUpdate; }
    
    const std::string& GetLabel() const { return m_label; }
    const DirectX::XMFLOAT4& GetTextColor() const { return m_textColor; }
    const DirectX::XMFLOAT4& GetBarColor() const { return m_barColor; }
    const DirectX::XMFLOAT4& GetBackgroundColor() const { return m_backgroundColor; }
    int GetBarWidth() const { return m_barWidth; }
    int GetBarHeight() const { return m_barHeight; }
    bool GetShowText() const { return m_showText; }
    bool GetShowValue() const { return m_showValue; }
    float GetCharacterSpacing() const { return m_characterSpacing; }
};

#endif // UI_BAR_H
