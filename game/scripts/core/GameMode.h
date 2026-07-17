/*****************************************************
 * File: game/scripts/core/GameMode.h
 * Description: Session-selected match mode (survives lobby).
 *****************************************************/
#pragma once

enum class GameMode {
    BattleRoyale = 0,
    ExtremeBattle = 1,
    Team2v2 = 2,
    Training = 3
};

GameMode GameMode_Get();
void GameMode_Set(GameMode mode);
bool GameMode_IsTraining();

const char* GameMode_MatchSceneName(GameMode mode);

/** Force lobby slots to Blue/Blue/Red/Red for Team2v2 tints. */
void GameMode_ApplyTeam2v2LobbyColors();
