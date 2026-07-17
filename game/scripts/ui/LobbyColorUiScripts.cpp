#include "LobbyColorUiScripts.h"

#include "../core/PlayerColor.h"
#include "engine/scripting/ui_canvas_script_register.h"
#include "engine/ui/ui_sprite.h"

#include <cctype>
#include <string>

REGISTER_UI_SCRIPT(LobbyColorPreviewScript)

namespace {
    int LobbySlotFromElementName(const std::string& name) {
        if (name.size() < 2 || name[0] != 'p' || !std::isdigit(static_cast<unsigned char>(name[1])))
            return -1;
        const int slot = name[1] - '1';
        if (slot < 0 || slot >= PlayerColor_LobbySlotCount)
            return -1;
        return slot;
    }
}

extern "C" __declspec(dllexport) void GameScripts_RotateLobbyColor(int slot) {
    if (slot < 0 || slot >= PlayerColor_LobbySlotCount)
        return;
    PlayerColor_RotateLobbySlot(slot);
}

void LobbyColorPreviewScript::OnCreate(UIElement& owner) {
    m_slot = LobbySlotFromElementName(owner.GetName());
    if (m_slot == 0)
        PlayerColor_ResetLobby();
}

void LobbyColorPreviewScript::OnUpdate(UIElement& owner, float dt) {
    (void)dt;
    if (m_slot < 0)
        return;

    auto* sprite = dynamic_cast<UISprite*>(&owner);
    if (!sprite)
        return;

    const DirectX::XMFLOAT4 tint = PlayerColor_GetTint(PlayerColor_GetLobbySlot(m_slot));
    sprite->SetColor(tint);
}
