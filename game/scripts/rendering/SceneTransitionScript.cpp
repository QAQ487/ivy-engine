#include "engine/scripting/script_runtime_api.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <random>
#include <string>

using namespace DirectX;

namespace {

enum class TransitionPhase {
    None,
    WipeOut,
    Loading,
    WipeIn,
};

constexpr float kWipeDuration = 0.5f;
constexpr float kMinTotalDuration = 5.0f;
constexpr float kFrameRotationDegPerSec = 45.0f;
constexpr float kIconRotationDegPerSec = 120.0f;
constexpr float kDelayInterval = 0.08f;
constexpr int kWipeBarCount = 5;

float s_wipeOutBarDelays[kWipeBarCount] = {};
float s_wipeInBarDelays[kWipeBarCount] = {};
std::mt19937 s_rng;

TransitionPhase s_phase = TransitionPhase::None;
std::string s_targetScene;
bool s_playOnLoad = false;
bool s_loadFinished = false;
bool s_loadAttempted = false;
double s_totalElapsed = 0.0;
double s_phaseElapsed = 0.0;
float s_frameAngle = 0.0f;
float s_iconAngle = 0.0f;

int s_barTexture = -1;
int s_frameTexture = -1;
int s_iconTexture = -1;

float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float GetBarDelay(int barIndex, bool wipeOut) {
    return wipeOut ? s_wipeOutBarDelays[barIndex] : s_wipeInBarDelays[barIndex];
}

void RandomizeBarDelays(float outDelays[kWipeBarCount]) {
    std::array<int, kWipeBarCount> barIndices = { 0, 1, 2, 3, 4 };
    std::shuffle(barIndices.begin(), barIndices.end(), s_rng);
    for (int slot = 0; slot < kWipeBarCount; ++slot)
        outDelays[barIndices[slot]] = kDelayInterval * static_cast<float>(slot);
}

int FindBarWithDelayExtreme(bool wipeOut, bool findMin) {
    int bestBar = 0;
    float bestDelay = GetBarDelay(0, wipeOut);
    for (int i = 1; i < kWipeBarCount; ++i) {
        const float delay = GetBarDelay(i, wipeOut);
        if (findMin ? delay < bestDelay : delay > bestDelay) {
            bestDelay = delay;
            bestBar = i;
        }
    }
    return bestBar;
}

float GetBarProgress(float delay, bool wipeOut) {
    const float t = static_cast<float>((s_phaseElapsed - delay) / kWipeDuration);
    const float progress = Clamp01(t);
    return wipeOut ? progress : (1.0f - progress);
}

float GetBarX(int barIndex, bool wipeOut) {
    const float screenW = static_cast<float>(ScriptApi_GetBackBufferWidth());
    const float barWidth = screenW;
    const float progress = GetBarProgress(GetBarDelay(barIndex, wipeOut), wipeOut);
    if (progress <= 0.0f)
        return -barWidth;
    return -barWidth + progress * (screenW + barWidth);
}

float GetFastestBarX(bool wipeOut) {
    return GetBarX(FindBarWithDelayExtreme(wipeOut, true), wipeOut);
}

float GetSlowestBarX(bool wipeOut) {
    return GetBarX(FindBarWithDelayExtreme(wipeOut, false), wipeOut);
}

float GetWhiteMaskWidth(bool wipeOut) {
    const float screenW = static_cast<float>(ScriptApi_GetBackBufferWidth());
    float maskWidth = wipeOut ? GetFastestBarX(wipeOut) : GetSlowestBarX(wipeOut);
    if (maskWidth < 0.0f)
        maskWidth = 0.0f;
    if (maskWidth > screenW)
        maskWidth = screenW;
    return maskWidth;
}

void DrawWhiteMask(float width) {
    if (width <= 0.0f)
        return;

    const float screenH = static_cast<float>(ScriptApi_GetBackBufferHeight());
    const int whiteTexture = ScriptApi_TextureGetWhiteTexture();
    if (whiteTexture < 0)
        return;

    const XMFLOAT4 white{ 1.0f, 1.0f, 1.0f, 1.0f };
    ScriptApi_SpriteDraw(whiteTexture, 0.0f, 0.0f, width, screenH, 0, 0, 1, 1, 0.0f, white);
}

bool AreAllBarsComplete(bool wipeOut) {
    for (int i = 0; i < kWipeBarCount; ++i) {
        const float progress = GetBarProgress(GetBarDelay(i, wipeOut), wipeOut);
        if (wipeOut) {
            if (progress < 1.0f)
                return false;
        }
        else if (progress > 0.0f) {
            return false;
        }
    }
    return true;
}

void DrawWipeBars(bool wipeOut) {
    if (s_barTexture < 0)
        return;

    const float screenW = static_cast<float>(ScriptApi_GetBackBufferWidth());
    const float screenH = static_cast<float>(ScriptApi_GetBackBufferHeight());
    const int barTexW = static_cast<int>(ScriptApi_TextureGetWidth(s_barTexture));
    const int barTexH = static_cast<int>(ScriptApi_TextureGetHeight(s_barTexture));
    const XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    const float barHeight = screenH / static_cast<float>(kWipeBarCount);
    const float barWidth = screenW;

    for (int i = 0; i < kWipeBarCount; ++i) {
        const float progress = GetBarProgress(GetBarDelay(i, wipeOut), wipeOut);
        if (progress <= 0.0f)
            continue;

        const float y = barHeight * static_cast<float>(i);
        const float x = -barWidth + progress * (screenW + barWidth);

        ScriptApi_SpriteDraw(s_barTexture, x, y, barWidth, barHeight,
            0, 0, barTexW, barTexH, 0.0f, color);
    }
}

void DrawLoadingScreen() {
    const float screenW = static_cast<float>(ScriptApi_GetBackBufferWidth());
    const float screenH = static_cast<float>(ScriptApi_GetBackBufferHeight());
    const XMFLOAT4 white{ 1.0f, 1.0f, 1.0f, 1.0f };

    const int whiteTexture = ScriptApi_TextureGetWhiteTexture();
    if (whiteTexture >= 0) {
        ScriptApi_SpriteDraw(whiteTexture, 0.0f, 0.0f, screenW, screenH,
            0, 0, 1, 1, 0.0f, white);
    }

    if (s_frameTexture >= 0) {
        const float fw = static_cast<float>(ScriptApi_TextureGetWidth(s_frameTexture));
        const float fh = static_cast<float>(ScriptApi_TextureGetHeight(s_frameTexture));
        const float x = screenW * 0.5f - fw * 0.5f;
        const float y = screenH * 0.5f - fh * 0.5f;
        ScriptApi_SpriteDraw(s_frameTexture, x, y, fw, fh,
            0, 0, static_cast<int>(fw), static_cast<int>(fh), s_frameAngle, white);
    }

    if (s_iconTexture >= 0) {
        const float iw = static_cast<float>(ScriptApi_TextureGetWidth(s_iconTexture));
        const float ih = static_cast<float>(ScriptApi_TextureGetHeight(s_iconTexture));
        const float x = screenW * 0.5f - iw * 0.5f;
        const float y = screenH * 0.5f - ih * 0.5f;
        ScriptApi_SpriteDraw(s_iconTexture, x, y, iw, ih,
            0, 0, static_cast<int>(iw), static_cast<int>(ih), s_iconAngle, white);
    }
}

void BeginLoadingPhase() {
    s_phase = TransitionPhase::Loading;
    s_phaseElapsed = 0.0;
    s_loadAttempted = false;
    s_loadFinished = false;
}

void AttemptSceneLoad() {
    if (s_loadAttempted || s_targetScene.empty())
        return;

    s_loadAttempted = true;
    s_loadFinished = ScriptApi_EngineGUILoadFromFile(s_targetScene.c_str());
}

void BeginWipeInPhase() {
    s_phase = TransitionPhase::WipeIn;
    s_phaseElapsed = 0.0;
    RandomizeBarDelays(s_wipeInBarDelays);
    if (s_playOnLoad) {
        ScriptApi_EngineGUIStartPlay();
        s_playOnLoad = false;
    }
}

bool PathContains(const char* path, const char* needle) {
    return path && needle && std::strstr(path, needle) != nullptr;
}

bool TryBeginInternal(const char* targetScene, bool playOnLoad) {
    if (s_phase != TransitionPhase::None || !targetScene || targetScene[0] == '\0')
        return false;

    s_targetScene = targetScene;
    s_playOnLoad = playOnLoad;
    s_loadFinished = false;
    s_loadAttempted = false;
    s_totalElapsed = 0.0;
    s_phaseElapsed = 0.0;
    s_frameAngle = 0.0f;
    s_iconAngle = 0.0f;
    RandomizeBarDelays(s_wipeOutBarDelays);
    s_phase = TransitionPhase::WipeOut;
    return true;
}

} // namespace

extern "C" __declspec(dllexport)
void SceneTransition_Initialize() {
    s_phase = TransitionPhase::None;
    s_targetScene.clear();
    s_playOnLoad = false;
    s_loadFinished = false;
    s_loadAttempted = false;
    s_totalElapsed = 0.0;
    s_phaseElapsed = 0.0;
    s_frameAngle = 0.0f;
    s_iconAngle = 0.0f;

    s_rng.seed(static_cast<unsigned>(std::random_device{}()));

    s_barTexture = ScriptApi_TextureLoad(L"rom/texture/transition/loading_bar.png", false);
    s_frameTexture = ScriptApi_TextureLoad(L"rom/texture/transition/Loading_frame.png", false);
    s_iconTexture = ScriptApi_TextureLoad(L"rom/texture/transition/Loading_icon.png", false);
}

extern "C" __declspec(dllexport)
void SceneTransition_Finalize() {
    if (s_barTexture >= 0) {
        ScriptApi_TextureRelease(s_barTexture);
        s_barTexture = -1;
    }
    if (s_frameTexture >= 0) {
        ScriptApi_TextureRelease(s_frameTexture);
        s_frameTexture = -1;
    }
    if (s_iconTexture >= 0) {
        ScriptApi_TextureRelease(s_iconTexture);
        s_iconTexture = -1;
    }
}

extern "C" __declspec(dllexport)
bool SceneTransition_IsActive() {
    return s_phase != TransitionPhase::None;
}

extern "C" __declspec(dllexport)
bool SceneTransition_BlocksSceneUpdate() {
    return s_phase == TransitionPhase::WipeOut || s_phase == TransitionPhase::Loading;
}

extern "C" __declspec(dllexport)
bool SceneTransition_TryBegin(const char* targetScene, bool playOnLoad) {
    return TryBeginInternal(targetScene, playOnLoad);
}

extern "C" __declspec(dllexport)
bool SceneTransition_TryHandleSceneRequest(const char* currentScene,
                                             const char* nextScene,
                                             bool playOnLoad) {
    if (!nextScene || nextScene[0] == '\0')
        return false;

    const bool fromTitle = PathContains(currentScene, "title.json");
    const bool toColorSelect = PathContains(nextScene, "color_select.json");
    if (fromTitle && toColorSelect)
        return TryBeginInternal(nextScene, playOnLoad);

    const bool fromLobby = PathContains(currentScene, "scene_lobby.json")
                        || PathContains(currentScene, "scene_2v2_confirm.json")
                        || PathContains(currentScene, "scene_mode_select.json");
    const bool toMatch = PathContains(nextScene, "scene_battle_royale.json")
                      || PathContains(nextScene, "scene_extreme_battle.json")
                      || PathContains(nextScene, "scene_2v2.json")
                      || PathContains(nextScene, "scene_training.json");
    if (fromLobby && toMatch)
        return TryBeginInternal(nextScene, playOnLoad);

    return false;
}

extern "C" __declspec(dllexport)
void SceneTransition_Update(double elapsed_time) {
    if (s_phase == TransitionPhase::None)
        return;

    s_totalElapsed += elapsed_time;
    s_phaseElapsed += elapsed_time;

    switch (s_phase) {
    case TransitionPhase::WipeOut:
        if (AreAllBarsComplete(true))
            BeginLoadingPhase();
        break;

    case TransitionPhase::Loading:
        if (s_loadAttempted && s_loadFinished && s_totalElapsed >= kMinTotalDuration)
            BeginWipeInPhase();
        break;

    case TransitionPhase::WipeIn:
        if (AreAllBarsComplete(false)) {
            s_phase = TransitionPhase::None;
            s_targetScene.clear();
            s_playOnLoad = false;
        }
        break;

    case TransitionPhase::None:
        break;
    }

    if (s_phase == TransitionPhase::WipeOut || s_phase == TransitionPhase::Loading) {
        s_frameAngle += kFrameRotationDegPerSec * static_cast<float>(elapsed_time);
        s_iconAngle += kIconRotationDegPerSec * static_cast<float>(elapsed_time);
    }

    if (s_phase == TransitionPhase::Loading)
        AttemptSceneLoad();
}

extern "C" __declspec(dllexport)
void SceneTransition_Draw() {
    switch (s_phase) {
    case TransitionPhase::WipeOut:
        DrawWhiteMask(GetWhiteMaskWidth(true));
        DrawWipeBars(true);
        break;
    case TransitionPhase::Loading:
        DrawLoadingScreen();
        break;
    case TransitionPhase::WipeIn:
        DrawWhiteMask(GetWhiteMaskWidth(false));
        DrawWipeBars(false);
        break;
    case TransitionPhase::None:
        break;
    }
}
