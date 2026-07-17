#include "game_score.h"

static int g_scores[kGameScoreMaxPlayers] = {};
static int g_deaths[kGameScoreMaxPlayers] = {};

void GameScore_Reset()
{
    for (int i = 0; i < kGameScoreMaxPlayers; ++i) {
        g_scores[i] = 0;
        g_deaths[i] = 0;
    }
}

void GameScore_Add(int playerIndex, int points)
{
    if (playerIndex < 0 || playerIndex >= kGameScoreMaxPlayers || points < 0) return;
    g_scores[playerIndex] += points;
}

void GameScore_Add(int points)
{
    GameScore_Add(0, points);
}

int GameScore_Get(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= kGameScoreMaxPlayers) return 0;
    return g_scores[playerIndex];
}

int GameScore_Get()
{
    return g_scores[0];
}

void GameScore_AddDeath(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= kGameScoreMaxPlayers) return;
    ++g_deaths[playerIndex];
}

int GameScore_GetDeaths(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= kGameScoreMaxPlayers) return 0;
    return g_deaths[playerIndex];
}
