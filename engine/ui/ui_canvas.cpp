/**===========================================================================
 File   :  [ui_canvas.cpp]
 Author : Copilot Agent
 Date   : 2025/11/17
 Brief  : Implementation of UI canvas container
 ===========================================================================**/

#include "ui_canvas.h"
#include "../input/controller.h"
#include <algorithm>
#include "../input/key_logger.h"

UICanvas::UICanvas(const std::string& name)
    : m_name(name)
    , m_selectedButtonIndex(-1)
    , m_navigationEnabled(false)
{
}

void UICanvas::Initialize() {
    for (auto& element : m_elements) {
        if (element) {
            element->Initialize();
        }
    }
    
    // Keyboard/controller navigation is temporarily disabled while mouse control is being fixed.
    if (m_navigationEnabled && !m_buttons.empty() && m_selectedButtonIndex < 0) {
        SelectButton(0);
    }
}

void UICanvas::Update(double elapsed_time) {
    // Update navigation if enabled
    if (m_navigationEnabled) {
        UpdateNavigation();
    }
    
    // Update all elements
    for (auto& element : m_elements) {
        if (element && element->IsVisible()) {
            element->Update(elapsed_time);
        }
    }
}

void UICanvas::Draw() {
    // Draw all visible elements
    for (auto& element : m_elements) {
        if (element && element->IsVisible()) {
            element->Draw();
        }
    }
}

void UICanvas::Finalize() {
    for (auto& element : m_elements) {
        if (element) {
            element->Finalize();
        }
    }
    m_elements.clear();
    m_buttons.clear();
    m_selectedButtonIndex = -1;
}

UIButton* UICanvas::AddButton(const std::string& name,
                              const DirectX::XMFLOAT2& position,
                              UIAnchor anchor,
                              float width,
                              float height) {
    auto button = std::make_unique<UIButton>(name, position, anchor, width, height);
    UIButton* buttonPtr = button.get();
    
    // Add to buttons list for navigation
    m_buttons.push_back(buttonPtr);
    
    // Add to elements list
    m_elements.push_back(std::move(button));
    
    if (m_navigationEnabled && m_buttons.size() == 1) {
        SelectButton(0);
    }
    
    return buttonPtr;
}

UISprite* UICanvas::AddSprite(const std::string& name,
                              const DirectX::XMFLOAT2& position,
                              UIAnchor anchor,
                              float width,
                              float height) {
    auto sprite = std::make_unique<UISprite>(name, position, anchor, width, height);
    UISprite* spritePtr = sprite.get();
    
    m_elements.push_back(std::move(sprite));
    
    return spritePtr;
}

UIBar* UICanvas::AddBar(const std::string& name,
                        const DirectX::XMFLOAT2& position,
                        UIAnchor anchor,
                        float maxValue,
                        const std::string& label) {
    auto bar = std::make_unique<UIBar>(name, position, anchor, maxValue, label);
    UIBar* barPtr = bar.get();
    
    m_elements.push_back(std::move(bar));
    
    return barPtr;
}

UIText* UICanvas::AddText(const std::string& name,
                         const DirectX::XMFLOAT2& position,
                         UIAnchor anchor,
                         const std::string& text) {
    auto textElement = std::make_unique<UIText>(name, position, anchor, text);
    UIText* textPtr = textElement.get();
    
    m_elements.push_back(std::move(textElement));
    
    return textPtr;
}

void UICanvas::AddElement(std::unique_ptr<UIElement> element) {
    if (!element) return;
    
    // Check if it's a button and add to navigation list
    UIButton* button = dynamic_cast<UIButton*>(element.get());
    if (button) {
        m_buttons.push_back(button);
        
        if (m_navigationEnabled && m_buttons.size() == 1) {
            SelectButton(0);
        }
    }
    
    m_elements.push_back(std::move(element));
}

void UICanvas::ClearElements() {
    for (auto& element : m_elements) {
        if (element)
            element->Finalize();
    }
    m_elements.clear();
    m_buttons.clear();
    m_selectedButtonIndex = -1;
}

void UICanvas::RemoveElement(const std::string& name) {
    // Find and remove from elements list
    auto it = std::find_if(m_elements.begin(), m_elements.end(),
        [&name](const std::unique_ptr<UIElement>& element) {
            return element && element->GetName() == name;
        });
    
    if (it != m_elements.end()) {
        // Check if it's a button and remove from buttons list
        UIButton* button = dynamic_cast<UIButton*>(it->get());
        if (button) {
            auto btnIt = std::find(m_buttons.begin(), m_buttons.end(), button);
            if (btnIt != m_buttons.end()) {
                m_buttons.erase(btnIt);
            }
            
            // Update selected index if needed
            if (m_selectedButtonIndex >= static_cast<int>(m_buttons.size())) {
                m_selectedButtonIndex = static_cast<int>(m_buttons.size()) - 1;
            }
        }
        
        (*it)->Finalize();
        m_elements.erase(it);
    }
}

void UICanvas::RemoveElement(UIElement* element) {
    if (!element) return;

    auto it = std::find_if(m_elements.begin(), m_elements.end(),
        [element](const std::unique_ptr<UIElement>& owned) {
            return owned.get() == element;
        });

    if (it == m_elements.end()) return;

    if (UIButton* button = dynamic_cast<UIButton*>(it->get())) {
        auto btnIt = std::find(m_buttons.begin(), m_buttons.end(), button);
        if (btnIt != m_buttons.end())
            m_buttons.erase(btnIt);
        if (m_selectedButtonIndex >= static_cast<int>(m_buttons.size()))
            m_selectedButtonIndex = static_cast<int>(m_buttons.size()) - 1;
    }

    (*it)->Finalize();
    m_elements.erase(it);
}

bool UICanvas::MoveElement(UIElement* element, int offset) {
    if (!element || offset == 0) return false;

    auto it = std::find_if(m_elements.begin(), m_elements.end(),
        [element](const std::unique_ptr<UIElement>& owned) {
            return owned.get() == element;
        });
    if (it == m_elements.end()) return false;

    const int index = static_cast<int>(std::distance(m_elements.begin(), it));
    int target = index + offset;
    if (target < 0) target = 0;
    const int last = static_cast<int>(m_elements.size()) - 1;
    if (target > last) target = last;
    if (target == index) return false;

    UIButton* selectedButton = GetSelectedButton();
    auto owned = std::move(*it);
    m_elements.erase(it);
    m_elements.insert(m_elements.begin() + target, std::move(owned));
    RebuildButtonList(selectedButton);
    return true;
}

bool UICanvas::MoveElementForward(UIElement* element) {
    return MoveElement(element, 1);
}

bool UICanvas::MoveElementBackward(UIElement* element) {
    return MoveElement(element, -1);
}

bool UICanvas::MoveElementToFront(UIElement* element) {
    if (!element) return false;
    return MoveElement(element, static_cast<int>(m_elements.size()));
}

bool UICanvas::MoveElementToBack(UIElement* element) {
    if (!element) return false;
    return MoveElement(element, -static_cast<int>(m_elements.size()));
}

UIElement* UICanvas::GetElement(const std::string& name) {
    auto it = std::find_if(m_elements.begin(), m_elements.end(),
        [&name](const std::unique_ptr<UIElement>& element) {
            return element && element->GetName() == name;
        });
    
    return (it != m_elements.end()) ? it->get() : nullptr;
}

void UICanvas::SelectButton(int index) {
    if (m_buttons.empty()) {
        m_selectedButtonIndex = -1;
        return;
    }
    
    // Deselect previous button
    if (m_selectedButtonIndex >= 0 && m_selectedButtonIndex < static_cast<int>(m_buttons.size())) {
        m_buttons[m_selectedButtonIndex]->SetSelected(false);
    }
    
    // Clamp index to valid range
    m_selectedButtonIndex = index;
    if (m_selectedButtonIndex < 0) {
        m_selectedButtonIndex = 0;
    }
    if (m_selectedButtonIndex >= static_cast<int>(m_buttons.size())) {
        m_selectedButtonIndex = static_cast<int>(m_buttons.size()) - 1;
    }
    
    // Select new button
    m_buttons[m_selectedButtonIndex]->SetSelected(true);
}

void UICanvas::SelectNextButton() {
    if (m_buttons.empty()) return;
    
    int nextIndex = m_selectedButtonIndex + 1;
    if (nextIndex >= static_cast<int>(m_buttons.size())) {
        nextIndex = 0;  // Wrap around to first button
    }
    
    SelectButton(nextIndex);
}

void UICanvas::SelectPreviousButton() {
    if (m_buttons.empty()) return;
    
    int prevIndex = m_selectedButtonIndex - 1;
    if (prevIndex < 0) {
        prevIndex = static_cast<int>(m_buttons.size()) - 1;  // Wrap around to last button
    }
    
    SelectButton(prevIndex);
}

UIButton* UICanvas::GetSelectedButton() {
    if (m_selectedButtonIndex >= 0 && m_selectedButtonIndex < static_cast<int>(m_buttons.size())) {
        return m_buttons[m_selectedButtonIndex];
    }
    return nullptr;
}

void UICanvas::EnableNavigation(bool enable) {
    m_navigationEnabled = enable;
    if (enable) {
        if (!m_buttons.empty() && m_selectedButtonIndex < 0)
            SelectButton(0);
        return;
    }

    if (m_selectedButtonIndex >= 0 && m_selectedButtonIndex < static_cast<int>(m_buttons.size()))
        m_buttons[m_selectedButtonIndex]->SetSelected(false);
    m_selectedButtonIndex = -1;
}

void UICanvas::SetVisible(bool visible) {
    for (auto& element : m_elements) {
        if (element) {
            element->SetVisible(visible);
        }
    }
}

void UICanvas::RebuildButtonList(UIButton* selectedButton) {
    m_buttons.clear();
    m_selectedButtonIndex = -1;

    for (const auto& element : m_elements) {
        if (auto* button = dynamic_cast<UIButton*>(element.get())) {
            if (button == selectedButton)
                m_selectedButtonIndex = static_cast<int>(m_buttons.size());
            m_buttons.push_back(button);
        }
    }
}

void UICanvas::UpdateNavigation() {
    if (m_buttons.empty()) return;
    
    UIButton* currentButton = GetSelectedButton();
    if (!currentButton) return;
    
    // Check for navigation input
    bool navigateDown = false;
    bool navigateUp = false;
    bool navigateLeft = false;
    bool navigateRight = false;
    
    // Keyboard navigation (4 directions)
    if (KeyLogger_IsTrigger(KK_DOWN)) {
        navigateDown = true;
    }
    if (KeyLogger_IsTrigger(KK_UP)) {
        navigateUp = true;
    }
    if (KeyLogger_IsTrigger(KK_LEFT)) {
        navigateLeft = true;
    }
    if (KeyLogger_IsTrigger(KK_RIGHT)) {
        navigateRight = true;
    }
    
    // Controller navigation (4 directions)
    if (Controller_IsTrigger(ControllerButton::DPadDown)) {
        navigateDown = true;
    }
    if (Controller_IsTrigger(ControllerButton::DPadUp)) {
        navigateUp = true;
    }
    if (Controller_IsTrigger(ControllerButton::DPadLeft)) {
        navigateLeft = true;
    }
    if (Controller_IsTrigger(ControllerButton::DPadRight)) {
        navigateRight = true;
    }
    
    // Handle manual navigation links first (if set)
    UIButton* targetButton = nullptr;
    
    if (navigateUp && currentButton->GetNextUp()) {
        targetButton = currentButton->GetNextUp();
    } else if (navigateDown && currentButton->GetNextDown()) {
        targetButton = currentButton->GetNextDown();
    } else if (navigateLeft && currentButton->GetNextLeft()) {
        targetButton = currentButton->GetNextLeft();
    } else if (navigateRight && currentButton->GetNextRight()) {
        targetButton = currentButton->GetNextRight();
    }
    
    // If manual link was used, find the button index and select it
    if (targetButton) {
        for (size_t i = 0; i < m_buttons.size(); ++i) {
            if (m_buttons[i] == targetButton) {
                SelectButton(static_cast<int>(i));
                return;
            }
        }
    }
    
    // Fallback to default sequential navigation (up/down only for backwards compatibility)
    if (navigateDown) {
        SelectNextButton();
    } else if (navigateUp) {
        SelectPreviousButton();
    }
}
