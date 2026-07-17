#include "PlayerColor.h"

#include <cstring>

namespace {
    PlayerColor g_playerColor = PlayerColor::Green;
    PlayerColor g_enemyColor = PlayerColor::Red;
    PlayerColor g_lobbyColors[PlayerColor_LobbySlotCount] = {
        PlayerColor::Green,
        PlayerColor::Red,
        PlayerColor::Blue,
        PlayerColor::Yellow
    };

    bool IsLobbyColorTaken(int slot, PlayerColor color) {
        for (int i = 0; i < PlayerColor_LobbySlotCount; ++i) {
            if (i != slot && g_lobbyColors[i] == color)
                return true;
        }
        return false;
    }

    void SyncLegacyColorsFromLobby() {
        g_playerColor = g_lobbyColors[0];
        g_enemyColor = g_lobbyColors[1];
    }
} // namespace

const char* PlayerColor_GetIconTexturePath(PlayerColor color) {
    switch (color) {
        case PlayerColor::Blue:
            return "rom/texture/UI/player_icon/player_blue/blue_player_icon.png";
        case PlayerColor::Green:
            return "rom/texture/UI/player_icon/player_green/green_player_icon.png";
        case PlayerColor::GreenBlue:
            return "rom/texture/UI/player_icon/player_greenblue/greenblue_player_icon.png";
        case PlayerColor::Orange:
            return "rom/texture/UI/player_icon/player_orange/orange_player_icon.png";
        case PlayerColor::Pink:
            return "rom/texture/UI/player_icon/player_pink/pink_player_icon.png";
        case PlayerColor::Purple:
            return "rom/texture/UI/player_icon/player_purple/purple_player_icon.png";
        case PlayerColor::Red:
            return "rom/texture/UI/player_icon/player_red/red_player_icon.png";
        case PlayerColor::Yellow:
            return "rom/texture/UI/player_icon/player_yellow/yellow_player_icon.png";
    }
    return "rom/texture/UI/player_icon/player_blue/blue_player_icon.png";
}

const wchar_t* PlayerColor_GetIconTexturePathW(PlayerColor color) {
    switch (color) {
        case PlayerColor::Blue:
            return L"rom/texture/UI/player_icon/player_blue/blue_player_icon.png";
        case PlayerColor::Green:
            return L"rom/texture/UI/player_icon/player_green/green_player_icon.png";
        case PlayerColor::GreenBlue:
            return L"rom/texture/UI/player_icon/player_greenblue/greenblue_player_icon.png";
        case PlayerColor::Orange:
            return L"rom/texture/UI/player_icon/player_orange/orange_player_icon.png";
        case PlayerColor::Pink:
            return L"rom/texture/UI/player_icon/player_pink/pink_player_icon.png";
        case PlayerColor::Purple:
            return L"rom/texture/UI/player_icon/player_purple/purple_player_icon.png";
        case PlayerColor::Red:
            return L"rom/texture/UI/player_icon/player_red/red_player_icon.png";
        case PlayerColor::Yellow:
            return L"rom/texture/UI/player_icon/player_yellow/yellow_player_icon.png";
    }
    return L"rom/texture/UI/player_icon/player_blue/blue_player_icon.png";
}

const char* PlayerColor_GetEnergyBarTexturePath(PlayerColor color) {
    switch (color) {
        case PlayerColor::Blue:
            return "rom/texture/UI/player_icon/player_blue/blue_player_energy_bar.png";
        case PlayerColor::Green:
            return "rom/texture/UI/player_icon/player_green/green_player_energy_bar.png";
        case PlayerColor::GreenBlue:
            return "rom/texture/UI/player_icon/player_greenblue/greenblue_player_energy_bar.png";
        case PlayerColor::Orange:
            return "rom/texture/UI/player_icon/player_orange/orange_player_energy_bar.png";
        case PlayerColor::Pink:
            return "rom/texture/UI/player_icon/player_pink/pink_player_energy_bar.png";
        case PlayerColor::Purple:
            return "rom/texture/UI/player_icon/player_purple/purple_player_energy_bar.png";
        case PlayerColor::Red:
            return "rom/texture/UI/player_icon/player_red/red_player_energy_bar.png";
        case PlayerColor::Yellow:
            return "rom/texture/UI/player_icon/player_yellow/yellow_player_energy_bar.png";
    }
    return "rom/texture/UI/player_icon/player_blue/blue_player_energy_bar.png";
}

const wchar_t* PlayerColor_GetEnergyBarTexturePathW(PlayerColor color) {
    switch (color) {
        case PlayerColor::Blue:
            return L"rom/texture/UI/player_icon/player_blue/blue_player_energy_bar.png";
        case PlayerColor::Green:
            return L"rom/texture/UI/player_icon/player_green/green_player_energy_bar.png";
        case PlayerColor::GreenBlue:
            return L"rom/texture/UI/player_icon/player_greenblue/greenblue_player_energy_bar.png";
        case PlayerColor::Orange:
            return L"rom/texture/UI/player_icon/player_orange/orange_player_energy_bar.png";
        case PlayerColor::Pink:
            return L"rom/texture/UI/player_icon/player_pink/pink_player_energy_bar.png";
        case PlayerColor::Purple:
            return L"rom/texture/UI/player_icon/player_purple/purple_player_energy_bar.png";
        case PlayerColor::Red:
            return L"rom/texture/UI/player_icon/player_red/red_player_energy_bar.png";
        case PlayerColor::Yellow:
            return L"rom/texture/UI/player_icon/player_yellow/yellow_player_energy_bar.png";
    }
    return L"rom/texture/UI/player_icon/player_blue/blue_player_energy_bar.png";
}

const char* PlayerColor_GetSuffixName(PlayerColor color) {
    switch (color) {
        case PlayerColor::Blue: return "Blue";
        case PlayerColor::Green: return "Green";
        case PlayerColor::GreenBlue: return "GreenBlue";
        case PlayerColor::Orange: return "Orange";
        case PlayerColor::Pink: return "Pink";
        case PlayerColor::Purple: return "Purple";
        case PlayerColor::Red: return "Red";
        case PlayerColor::Yellow: return "Yellow";
    }
    return "Blue";
}

PlayerColor PlayerColor_FromSuffix(const char* suffix) {
    if (!suffix || suffix[0] == '\0')
        return PlayerColor::Blue;
    if (std::strcmp(suffix, "Blue") == 0) return PlayerColor::Blue;
    if (std::strcmp(suffix, "Green") == 0) return PlayerColor::Green;
    if (std::strcmp(suffix, "GreenBlue") == 0) return PlayerColor::GreenBlue;
    if (std::strcmp(suffix, "Orange") == 0) return PlayerColor::Orange;
    if (std::strcmp(suffix, "Pink") == 0) return PlayerColor::Pink;
    if (std::strcmp(suffix, "Purple") == 0) return PlayerColor::Purple;
    if (std::strcmp(suffix, "Red") == 0) return PlayerColor::Red;
    if (std::strcmp(suffix, "Yellow") == 0) return PlayerColor::Yellow;
    return PlayerColor::Blue;
}

PlayerColor PlayerColor_FromIndex(int index) {
    int i = index % PlayerColor_Count;
    if (i < 0)
        i += PlayerColor_Count;
    return static_cast<PlayerColor>(i);
}

int PlayerColor_ToIndex(PlayerColor color) {
    const int index = static_cast<int>(color);
    if (index < 0 || index >= PlayerColor_Count)
        return 0;
    return index;
}

void PlayerColor_ResetSession() {
    PlayerColor_ResetLobby();
}

void PlayerColor_ResetLobby() {
    g_lobbyColors[0] = PlayerColor::Green;
    g_lobbyColors[1] = PlayerColor::Red;
    g_lobbyColors[2] = PlayerColor::Blue;
    g_lobbyColors[3] = PlayerColor::Yellow;
    SyncLegacyColorsFromLobby();
}

PlayerColor PlayerColor_GetLobbySlot(int slot) {
    if (slot < 0 || slot >= PlayerColor_LobbySlotCount)
        return PlayerColor::Blue;
    return g_lobbyColors[slot];
}

void PlayerColor_SetLobbySlot(int slot, PlayerColor color) {
    if (slot < 0 || slot >= PlayerColor_LobbySlotCount)
        return;
    g_lobbyColors[slot] = color;
    SyncLegacyColorsFromLobby();
}

void PlayerColor_RotateLobbySlot(int slot) {
    if (slot < 0 || slot >= PlayerColor_LobbySlotCount)
        return;

    const int currentIndex = PlayerColor_ToIndex(g_lobbyColors[slot]);
    for (int offset = 1; offset < PlayerColor_Count; ++offset) {
        const PlayerColor candidate = PlayerColor_FromIndex(currentIndex + offset);
        if (!IsLobbyColorTaken(slot, candidate)) {
            PlayerColor_SetLobbySlot(slot, candidate);
            return;
        }
    }
}

PlayerColor PlayerColor_GetPlayer() {
    return g_playerColor;
}

PlayerColor PlayerColor_GetEnemy() {
    return g_enemyColor;
}

void PlayerColor_SetEnemy(PlayerColor color) {
    g_enemyColor = color;
}

bool PlayerColor_TrySetEnemy(PlayerColor color) {
    if (color == g_playerColor)
        return false;
    g_enemyColor = color;
    return true;
}

void PlayerColor_SetPlayer(PlayerColor color) {
    g_playerColor = color;
    if (g_enemyColor == color) {
        const int playerIndex = PlayerColor_ToIndex(color);
        for (int offset = 1; offset < PlayerColor_Count; ++offset) {
            const PlayerColor next = PlayerColor_FromIndex(playerIndex + offset);
            if (next != color) {
                g_enemyColor = next;
                break;
            }
        }
    }
}

DirectX::XMFLOAT4 PlayerColor_GetEmissionGlow(PlayerColor color) {
    return PlayerColor_GetTint(color);
}

DirectX::XMFLOAT4 PlayerColor_GetTint(PlayerColor color) {
    switch (color) {
        case PlayerColor::Blue: return {0.2f, 0.45f, 1.0f, 1.0f};
        case PlayerColor::Green: return {0.2f, 0.85f, 0.25f, 1.0f};
        case PlayerColor::GreenBlue: return {0.2f, 0.9f, 0.7f, 1.0f};
        case PlayerColor::Orange: return {1.0f, 0.5f, 0.1f, 1.0f};
        case PlayerColor::Pink: return {1.0f, 0.3f, 0.7f, 1.0f};
        case PlayerColor::Purple: return {0.7f, 0.2f, 1.0f, 1.0f};
        case PlayerColor::Red: return {1.0f, 0.2f, 0.2f, 1.0f};
        case PlayerColor::Yellow: return {1.0f, 0.92f, 0.15f, 1.0f};
    }
    return {0.2f, 0.45f, 1.0f, 1.0f};
}
