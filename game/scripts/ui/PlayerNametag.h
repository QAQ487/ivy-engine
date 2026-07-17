/*****************************************************
 * File: game/scripts/ui/PlayerNametag.h
 * Brief: World billboard nametags (YOU / P2 / P3 / P4)
 *****************************************************/
#pragma once

#include <DirectXMath.h>

const char* PlayerNametag_LabelForIndex(int playerIndex);
void PlayerNametag_Draw(int playerIndex, const DirectX::XMFLOAT3& worldFeetPos);
