// game_scene_script_hooks.h
// App: game scene script hooks declarations.

#pragma once

bool GameSceneScript_InvokeBool(const char* exportName);
void GameSceneScript_Invoke(const char* exportName);
void GameSceneScript_InvokeWithDouble(const char* exportName, double value);
