#pragma once
// Debug-only host integration check. No keyboard injection or production UI.
#include "engine/core/engine_gui.h"
#include "engine/scene/scene_loader.h"
#include "engine/ui/scene_canvas.h"
#include "engine/rendering/d3d11/renderer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include "imgui/imgui_internal.h"
inline unsigned PlaySceneSwitchCheck_HeldDraws=0;
inline void PlaySceneSwitchCheck_Tick() {
    static const char* option=std::getenv("IVY_PLAY_SCENE_SWITCH_CHECK");
    if(!option)return;
    static int phase=0,frames=0,settled=0;
    if(phase<0)return;
    const bool fullscreen=std::strcmp(option,"fullscreen")==0;
    auto fail=[&](const char* message) {
        std::fprintf(stderr,"PLAY_SCENE_SWITCH_CHECK FAIL: %s mode=%d gui=%d playing=%d busy=%d\n",message,
            int(EngineGUI_GetPlayMode()),int(EngineGUI_IsEnabled()),int(EngineGUI_IsPlaying()),int(EngineGUI_IsSceneAssetPreloadActive()));
        std::fflush(stderr);phase=-1;PostQuitMessage(2);
    };
    if(!fullscreen&&std::strcmp(option,"viewport")!=0) {fail("expected fullscreen or viewport");return;}
    if(++frames>450) {fail("timed out");return;}
    const auto expected=fullscreen?PlayMode::Fullscreen:PlayMode::Viewport;
    const auto scene=std::filesystem::path(SceneLoader_GetCurrentScenePath()).filename().string();
    if(phase==0) {
        if(EngineGUI_IsSceneAssetPreloadActive())return;
        if(scene!="scene_topdown_2d.json") {fail("open TestProject with scene_topdown_2d.json first");return;}
        EngineGUI_StartPlay(expected);phase=1;settled=0;
    }
    if(EngineGUI_GetPlayMode()!=expected||EngineGUI_IsEnabled()==fullscreen||!EngineGUI_PreferPlayVisuals()) {
        fail("play presentation changed during scene load");return;
    }
    float x,y,w,h;EngineGUI_GetGameViewportRect(x,y,w,h);
    if(fullscreen ? (x!=0||y!=0||w!=GetBackBufferWidth()||h!=GetBackBufferHeight()) : (x<=0||y<=0||w>=GetBackBufferWidth())) {
        fail("viewport rectangle changed mode");return;
    }
    if(EngineGUI_IsSceneAssetPreloadActive()||!EngineGUI_IsPlaying()) {settled=0;return;}
    if(++settled<5)return;
    if(phase==1) {
        SceneCanvas_RequestSceneFile("scene_topdown_2d_next.json",true);
        phase=2;settled=0;return;
    }
    if(phase==2&&scene=="scene_topdown_2d_next.json") {
        if(!EngineGUI_LoadFromFile("rom/scene/scene_topdown_2d.json")) {fail("return scene load refused");return;}
        // Exercise the default API too: it must retain the held mode.
        EngineGUI_StartPlay();phase=3;settled=0;return;
    }
    if(phase==3&&scene=="scene_topdown_2d.json") {
        if(!PlaySceneSwitchCheck_HeldDraws) {fail("no pending-load draw observed");return;}
        std::printf("PLAY_SCENE_SWITCH_CHECK PASS: %s Canvas request + cached default API return; mode/gui/viewport preserved\n",option);
        std::fflush(stdout);phase=-1;EngineGUI_StopPlay();PostQuitMessage(0);
    }
}

inline void PlaySceneSwitchCheck_AfterDraw() {
    static const char* option=std::getenv("IVY_PLAY_SCENE_SWITCH_CHECK");
    if(!option||EngineGUI_IsPlaying()||!EngineGUI_PreferPlayVisuals()||!EngineGUI_IsSceneAssetPreloadActive())return;
    ++PlaySceneSwitchCheck_HeldDraws;
    auto* modal=ImGui::FindWindowByName("SceneAssetPreloadModal");
    if(modal&&modal->Active) {
        std::fprintf(stderr,"PLAY_SCENE_SWITCH_CHECK FAIL: whole-editor loading modal rendered during play transition\n");
        std::fflush(stderr);PostQuitMessage(2);
    }
}
