/**===========================================================================
 File   :  [ui_canvas.h]
 Author : Copilot Agent
 Date   : 2025/11/17
 Brief  : Container for managing UI elements in a scene with easy API
 ===========================================================================**/

#ifndef UI_CANVAS_H
#define UI_CANVAS_H

#include "ui_element.h"
#include "ui_button.h"
#include "ui_sprite.h"
#include "ui_bar.h"
#include "ui_text.h"
#include <memory>
#include <vector>
#include <string>
#include <DirectXMath.h>

/**
 * Canvas for managing UI elements in a scene.
 * Provides easy-to-use API for adding and managing UI components.
 * Also handles button navigation via keyboard/controller.
 */
class UICanvas {
private:
    std::string m_name;
    std::vector<std::unique_ptr<UIElement>> m_elements;
    std::vector<UIButton*> m_buttons;  // Quick access to buttons for navigation
    int m_selectedButtonIndex;
    bool m_navigationEnabled;
    
public:
    UICanvas(const std::string& name);
    ~UICanvas() = default;
    
    // Initialize all elements in the canvas
    void Initialize();
    
    // Update all elements and handle navigation
    void Update(double elapsed_time);
    
    // Draw all visible elements
    void Draw();
    
    // Cleanup all elements
    void Finalize();
    
    // Add UI elements with simple API
    UIButton* AddButton(const std::string& name,
                       const DirectX::XMFLOAT2& position,
                       UIAnchor anchor = UIAnchor::CENTER,
                       float width = 200.0f,
                       float height = 60.0f);
    
    UISprite* AddSprite(const std::string& name,
                       const DirectX::XMFLOAT2& position,
                       UIAnchor anchor = UIAnchor::CENTER,
                       float width = 64.0f,
                       float height = 64.0f);
    
    UIBar* AddBar(const std::string& name,
                  const DirectX::XMFLOAT2& position,
                  UIAnchor anchor = UIAnchor::TOP_LEFT,
                  float maxValue = 100.0f,
                  const std::string& label = "");
    
    UIText* AddText(const std::string& name,
                   const DirectX::XMFLOAT2& position,
                   UIAnchor anchor = UIAnchor::CENTER,
                   const std::string& text = "");
    
    // Generic add element method
    void AddElement(std::unique_ptr<UIElement> element);
    void InsertElementAt(std::unique_ptr<UIElement> element, int index);
    void ClearElements();
    
    // Remove an element by name
    void RemoveElement(const std::string& name);
    void RemoveElement(UIElement* element);
    int GetElementIndex(const UIElement* element) const;
    bool MoveElementToIndex(UIElement* element, int index);

    // Draw order helpers. Later elements draw on top of earlier elements.
    bool MoveElement(UIElement* element, int offset);
    bool MoveElementForward(UIElement* element);
    bool MoveElementBackward(UIElement* element);
    bool MoveElementToFront(UIElement* element);
    bool MoveElementToBack(UIElement* element);
    
    // Get an element by name
    UIElement* GetElement(const std::string& name);
    
    // Button navigation
    void EnableNavigation(bool enable);
    bool IsNavigationEnabled() const { return m_navigationEnabled; }
    
    void SelectButton(int index);
    void SelectNextButton();
    void SelectPreviousButton();
    int GetSelectedButtonIndex() const { return m_selectedButtonIndex; }
    UIButton* GetSelectedButton();
    
    // Canvas properties
    const std::string& GetName() const { return m_name; }
    size_t GetElementCount() const { return m_elements.size(); }
    size_t GetButtonCount() const { return m_buttons.size(); }
    const std::vector<std::unique_ptr<UIElement>>& GetElements() const { return m_elements; }
    
    // Visibility
    void SetVisible(bool visible);
    
private:
    void RebuildButtonList(UIButton* selectedButton = nullptr);

    // Update button navigation input
    void UpdateNavigation();
};

#endif // UI_CANVAS_H
