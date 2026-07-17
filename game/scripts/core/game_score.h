#pragma once

static constexpr int kGameScoreMaxPlayers = 4;

void GameScore_Reset();
void GameScore_Add(int playerIndex, int points);
void GameScore_Add(int points);
int  GameScore_Get(int playerIndex);
int  GameScore_Get();

void GameScore_AddDeath(int playerIndex);
int  GameScore_GetDeaths(int playerIndex);
