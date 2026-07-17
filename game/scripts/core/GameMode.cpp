/*****************************************************
 * File: game/scripts/core/GameMode.cpp
 * Description: Session GameMode + mode-select / lobby button exports.
 *****************************************************/
#include "GameMode.h"
#include "PlayerColor.h"
#include "engine/scripting/script_runtime_api.h"

namespace {

GameMode g_gameMode = GameMode::BattleRoyale;

} // namespace

GameMode GameMode_Get() {
    return g_gameMode;
}

void GameMode_Set(GameMode mode) {
    g_gameMode = mode;
}

bool GameMode_IsTraining() {
    return g_gameMode == GameMode::Training;
}

const char* GameMode_MatchSceneName(GameMode mode) {
    switch (mode) {
        case GameMode::ExtremeBattle:
            return "scene_extreme_battle";
        case GameMode::Team2v2:
            return "scene_2v2";
        case GameMode::Training:
            return "scene_training";
        case GameMode::BattleRoyale:
        default:
            return "scene_battle_royale";
    }
}

void GameMode_ApplyTeam2v2LobbyColors() {
    PlayerColor_SetLobbySlot(0, PlayerColor::Blue);
    PlayerColor_SetLobbySlot(1, PlayerColor::Blue);
    PlayerColor_SetLobbySlot(2, PlayerColor::Red);
    PlayerColor_SetLobbySlot(3, PlayerColor::Red);
}

extern "C" __declspec(dllexport) void GameScripts_SelectBattleRoyale() {
    GameMode_Set(GameMode::BattleRoyale);
    ScriptApi_SceneCanvasRequestSceneFile("scene_lobby", true);
}

extern "C" __declspec(dllexport) void GameScripts_SelectExtremeBattle() {
    GameMode_Set(GameMode::ExtremeBattle);
    ScriptApi_SceneCanvasRequestSceneFile("scene_lobby", true);
}

extern "C" __declspec(dllexport) void GameScripts_Select2v2() {
    GameMode_Set(GameMode::Team2v2);
    GameMode_ApplyTeam2v2LobbyColors();
    ScriptApi_SceneCanvasRequestSceneFile("scene_2v2_confirm", true);
}

extern "C" __declspec(dllexport) void GameScripts_Start2v2Match() {
    GameMode_Set(GameMode::Team2v2);
    GameMode_ApplyTeam2v2LobbyColors();
    ScriptApi_SceneCanvasRequestSceneFile(GameMode_MatchSceneName(GameMode::Team2v2), true);
}

extern "C" __declspec(dllexport) void GameScripts_SelectTraining() {
    GameMode_Set(GameMode::Training);
    ScriptApi_SceneCanvasRequestSceneFile(GameMode_MatchSceneName(GameMode::Training), true);
}

extern "C" __declspec(dllexport) void GameScripts_LoadSelectedModeMatch() {
    ScriptApi_SceneCanvasRequestSceneFile(GameMode_MatchSceneName(GameMode_Get()), true);
}
