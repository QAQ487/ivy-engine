#pragma once

#include <DirectXMath.h>

enum class PlayerColor {
    Blue,
    Green,
    GreenBlue,
    Orange,
    Pink,
    Purple,
    Red,
    Yellow
};

constexpr int PlayerColor_Count = 8;
constexpr int PlayerColor_LobbySlotCount = 4;

const char*    PlayerColor_GetIconTexturePath(PlayerColor color);
const wchar_t* PlayerColor_GetIconTexturePathW(PlayerColor color);
const char*    PlayerColor_GetEnergyBarTexturePath(PlayerColor color);
const wchar_t* PlayerColor_GetEnergyBarTexturePathW(PlayerColor color);
const char*    PlayerColor_GetSuffixName(PlayerColor color);
PlayerColor    PlayerColor_FromSuffix(const char* suffix);
PlayerColor    PlayerColor_FromIndex(int index);
int            PlayerColor_ToIndex(PlayerColor color);

void PlayerColor_ResetSession();
void PlayerColor_ResetLobby();
PlayerColor PlayerColor_GetLobbySlot(int slot);
void PlayerColor_SetLobbySlot(int slot, PlayerColor color);
void PlayerColor_RotateLobbySlot(int slot);
PlayerColor PlayerColor_GetPlayer();
PlayerColor PlayerColor_GetEnemy();
void PlayerColor_SetPlayer(PlayerColor color);
void PlayerColor_SetEnemy(PlayerColor color);
bool PlayerColor_TrySetEnemy(PlayerColor color);
DirectX::XMFLOAT4 PlayerColor_GetEmissionGlow(PlayerColor color);
DirectX::XMFLOAT4 PlayerColor_GetTint(PlayerColor color);
