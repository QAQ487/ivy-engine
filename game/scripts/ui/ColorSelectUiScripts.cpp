#include "ColorSelectUiScripts.h"

#include "../core/PlayerColor.h"
#include "engine/scripting/ui_canvas_script_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "engine/ui/ui_button.h"
#include "engine/ui/ui_sprite.h"

#include <cmath>
#include <cstring>
#include <string>

REGISTER_UI_SCRIPT(ColorPreviewScript)
REGISTER_UI_SCRIPT(ColorHighlightScript)
REGISTER_UI_SCRIPT(ColorEnemyButtonScript)

namespace {
    constexpr DirectX::XMFLOAT4 kHighlightTint = {1.0f, 0.95f, 0.2f, 0.85f};
    constexpr DirectX::XMFLOAT4 kEnemyEnabledTint = {1.0f, 1.0f, 1.0f, 1.0f};
    constexpr DirectX::XMFLOAT4 kEnemyBlockedTint = {1.0f, 1.0f, 1.0f, 0.35f};

    bool NameStartsWith(const std::string& name, const char* prefix) {
        return name.rfind(prefix, 0) == 0;
    }

    UIButton* FindButtonByName(const char* buttonName) {
        UICanvas* canvas = ScriptApi_SceneCanvasGetCanvas();
        if (!canvas || !buttonName)
            return nullptr;
        UIElement* element = ScriptApi_UICanvasGetElement(*canvas, buttonName);
        return element ? dynamic_cast<UIButton*>(element) : nullptr;
    }
}

extern "C" __declspec(dllexport) void GameScripts_PickPlayerColor(const char* suffix) {
    if (!suffix || suffix[0] == '\0')
        return;
    PlayerColor_SetPlayer(PlayerColor_FromSuffix(suffix));
}

extern "C" __declspec(dllexport) void GameScripts_PickEnemyColor(const char* suffix) {
    if (!suffix || suffix[0] == '\0')
        return;
    PlayerColor_TrySetEnemy(PlayerColor_FromSuffix(suffix));
}

void ColorPreviewScript::OnCreate(UIElement& owner) {
    if (owner.GetName() == "PlayerPreview")
        PlayerColor_ResetSession();
}

void ColorPreviewScript::OnUpdate(UIElement& owner, float dt) {
    (void)dt;
    auto* sprite = dynamic_cast<UISprite*>(&owner);
    if (!sprite)
        return;

    const std::string& name = owner.GetName();
    PlayerColor color = PlayerColor::Blue;
    if (name == "PlayerPreview")
        color = PlayerColor_GetPlayer();
    else if (name == "EnemyPreview")
        color = PlayerColor_GetEnemy();
    else
        return;

    ScriptApi_UISpriteSetTexture(*sprite, PlayerColor_GetIconTexturePathW(color));
}

void ColorHighlightScript::OnUpdate(UIElement& owner, float dt) {
    (void)dt;
    auto* highlight = dynamic_cast<UISprite*>(&owner);
    if (!highlight)
        return;

    const std::string& name = owner.GetName();
    std::string buttonName;
    if (name == "PlayerHighlight")
        buttonName = std::string("PickPlayer_") + PlayerColor_GetSuffixName(PlayerColor_GetPlayer());
    else if (name == "EnemyHighlight")
        buttonName = std::string("PickEnemy_") + PlayerColor_GetSuffixName(PlayerColor_GetEnemy());
    else
        return;

    UIButton* button = FindButtonByName(buttonName.c_str());
    if (!button) {
        highlight->SetVisible(false);
        return;
    }

    highlight->SetVisible(true);
    highlight->SetPosition(button->GetPosition());
    highlight->SetAnchor(button->GetAnchor());
    highlight->SetSize(80.0f, 80.0f);
    highlight->SetColor(kHighlightTint);
}

void ColorEnemyButtonScript::OnCreate(UIElement& owner) {
    m_isEnemyButton = NameStartsWith(owner.GetName(), "PickEnemy_");
}

void ColorEnemyButtonScript::OnUpdate(UIElement& owner, float dt) {
    (void)dt;
    if (!m_isEnemyButton)
        return;

    auto* button = dynamic_cast<UIButton*>(&owner);
    if (!button)
        return;

    const char* prefix = "PickEnemy_";
    const std::string suffix = owner.GetName().c_str() + std::strlen(prefix);
    const PlayerColor buttonColor = PlayerColor_FromSuffix(suffix.c_str());
    const bool blocked = buttonColor == PlayerColor_GetPlayer();
    button->SetColor(blocked ? kEnemyBlockedTint : kEnemyEnabledTint);
}
