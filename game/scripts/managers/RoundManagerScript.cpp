#include "RoundManagerScript.h"

#include "IntroCameraScript.h"
#include "EnemyAiScript.h"
#include "PlayerScript.h"
#include "game_score.h"
#include "GameMode.h"
#include "PlayerColor.h"
#include "engine/physics/collider/collider.h"
#include "engine/core/engine_gui.h"
#include "engine/core/game_object.h"
#include "engine/ecs/collider_component.h"
#include "engine/ecs/transform.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "engine/ui/scene_canvas.h"
#include "engine/ui/ui_text.h"
#include "engine/ui/ui_sprite.h"
#include "engine/ui/ui_button.h"
#include "camera.h"
#include "third_person_camera.h"
#include "scene_manager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

REGISTER_SCRIPT(RoundManagerScript)

namespace {
RoundManagerScript* g_activeRoundManager = nullptr;

bool TryFindArenaFloor(DirectX::XMFLOAT3& floorCenter, DirectX::XMFLOAT3& floorSize) {
    floorCenter = {0.0f, 0.0f, 0.0f};
    floorSize = {20.0f, 1.0f, 20.0f};

    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (!scene) return false;

    float bestArea = -1.0f;
    bool found = false;
    for (GameObject* obj : scene->GetObjects()) {
        if (!obj || !obj->IsActive() || obj->GetTag() != Tag::TAG_GROUND) continue;
        Collider* collider = obj->GetCollider();
        if (!collider) continue;

        XMFLOAT3 center = {};
        XMFLOAT3 size = {1.0f, 1.0f, 1.0f};
        if (!ScriptApi_TryGetColliderFootprint(*collider, center, size))
            continue;

        const float area = size.x * size.z;
        if (area > bestArea) {
            bestArea = area;
            floorCenter = center;
            floorSize = size;
            found = true;
        }
    }
    return found;
}

bool TryPickSpawnAtFixedRadius(float spawnY, DirectX::XMFLOAT3& outPosition) {
    XMFLOAT3 floorCenter = {0.0f, 0.0f, 0.0f};
    XMFLOAT3 floorSize = {20.0f, 1.0f, 20.0f};
    (void)TryFindArenaFloor(floorCenter, floorSize);

    const float angle = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 6.2831853f;
    const float radius = kSpawnRadiusFromCenter;

    outPosition = {
        floorCenter.x + radius * cosf(angle),
        spawnY,
        floorCenter.z + radius * sinf(angle)
    };
    return true;
}
} // namespace

bool RoundManager_TryPickRandomSpawnPosition(XMFLOAT3& outPosition) {
    return TryPickSpawnAtFixedRadius(outPosition.y, outPosition);
}

void RoundManager_EnsureMinSpawnRadius(XMFLOAT3& inOutPosition) {
    XMFLOAT3 picked = inOutPosition;
    TryPickSpawnAtFixedRadius(inOutPosition.y, picked);
    inOutPosition.x = picked.x;
    inOutPosition.z = picked.z;
}

bool RoundManager_IsGameplayActive()
{
    if (!EngineGUI_IsPlaying())
        return false;
    if (!g_activeRoundManager)
        return true;
    return g_activeRoundManager->IsGameplayActive();
}

bool RoundManager_IsPlayerEligibleForScoring(int playerIndex)
{
    if (!EngineGUI_IsPlaying())
        return false;
    if (!g_activeRoundManager)
        return true;
    return g_activeRoundManager->IsPlayerEligibleForScoring(playerIndex);
}

namespace {
    float Lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

    PlayerColor HudColorForPlayerIndex(int playerIndex) {
        if (playerIndex >= 0 && playerIndex < PlayerColor_LobbySlotCount)
            return PlayerColor_GetLobbySlot(playerIndex);
        return PlayerColor_GetPlayer();
    }

    void ApplyFloorColliderSize(GameObject* floor, const DirectX::XMFLOAT3& size) {
        if (!floor) return;

        if (auto* box = dynamic_cast<BoxCollider*>(floor->GetCollider())) {
            ScriptApi_SetBoxColliderSize(*box, size);
            return;
        }

        if (ColliderComponent* cc = ScriptApi_FindColliderComponent(*floor)) {
            if (cc->colliderType == ColliderType::CYLINDER) {
                cc->radius = size.x * 0.5f;
                cc->height = size.y;
                ScriptApi_ApplyColliderComponent(*cc);
            }
        }
    }

    DirectX::XMFLOAT3 GetFloorColliderSize(GameObject* floor, const DirectX::XMFLOAT3& fallback) {
        if (!floor) return fallback;

        Collider* collider = floor->GetCollider();
        if (!collider) return fallback;

        DirectX::XMFLOAT3 center = {};
        DirectX::XMFLOAT3 size = fallback;
        if (ScriptApi_TryGetColliderFootprint(*collider, center, size))
            return size;
        return fallback;
    }
}

void RoundManagerScript::ApplyFloorVisualScale(const DirectX::XMFLOAT3& scale)
{
    if (!m_floorObject) return;

    m_floorObject->SetScale(scale);
    if (engine::Transform* transform = ScriptApi_FindTransform(*m_floorObject))
        transform->localScale = scale;
}

DirectX::XMFLOAT3 RoundManagerScript::GetFloorVisualScale() const
{
    if (!m_floorObject)
        return {1.0f, 1.0f, 1.0f};

    if (const engine::Transform* transform = ScriptApi_FindTransform(*m_floorObject))
        return transform->localScale;
    return m_floorObject->GetScale();
}

void RoundManagerScript::OnCreate(engine::GameObject& self)
{
    (void)self;
    g_activeRoundManager = this;
    m_floorObject = nullptr;
    m_remainingTimeSeconds = gameDurationSeconds;
    m_shrinkAnimElapsed = 0.0f;
    m_wasPlaying = false;
    m_isPreStartActive = false;
    m_isGameFinished = false;
    m_preStartTimer = 0.0f;
    m_goFlashTimer = -1.0f;
    m_floorBaselineCaptured = false;
    m_isShrinkAnimating = false;
    m_shrunkAt50Percent = false;
    m_shrunkAt75Percent = false;
    m_scoreText = nullptr;
    m_timerText = nullptr;
    m_scoreBarBg = nullptr;
    m_statusPanelBg = nullptr;
    m_statusPanelBorder = nullptr;
    for (int i = 0; i < kGameScoreMaxPlayers; ++i) {
        m_scoreBars[i] = nullptr;
        m_scoreTexts[i] = nullptr;
        m_hudPlayerIcons[i] = nullptr;
        m_hudPlayerEnergyBars[i] = nullptr;
        m_hudPlayerNames[i] = nullptr;
        m_hudPlayerStatuses[i] = nullptr;
        m_resultPanels[i] = nullptr;
        m_resultScores[i] = nullptr;
        m_resultDeaths[i] = nullptr;
    }
    m_backButton = nullptr;
    m_playerRespawnTimer = -1.0f;
    m_enemyRespawns.clear();

    BindHudElements();
    BindResultOverlayElements();
    HideResultOverlay();
}

void RoundManagerScript::ApplyHudPlayerColorTextures()
{
    const bool teamMode = GameMode_Get() == GameMode::Team2v2;
    for (int i = 0; i < kGameScoreMaxPlayers; ++i) {
        const PlayerColor color = HudColorForPlayerIndex(i);
        if (m_hudPlayerIcons[i]) {
            ScriptApi_UISpriteSetTexture(*m_hudPlayerIcons[i],
                PlayerColor_GetIconTexturePathW(color));
        }
        if (m_hudPlayerEnergyBars[i]) {
            ScriptApi_UISpriteSetTexture(*m_hudPlayerEnergyBars[i],
                PlayerColor_GetEnergyBarTexturePathW(color));
        }
        if (!teamMode && m_scoreBars[i]) {
            m_scoreBars[i]->SetColor(PlayerColor_GetTint(color));
        }
    }

    if (teamMode) {
        if (m_scoreBars[0])
            m_scoreBars[0]->SetColor(PlayerColor_GetTint(PlayerColor::Blue));
        if (m_scoreBars[1])
            m_scoreBars[1]->SetColor(PlayerColor_GetTint(PlayerColor::Red));
    }
}

void RoundManagerScript::BindHudElements()
{
    UICanvas* canvas = ScriptApi_SceneCanvasGetCanvas();
    if (!canvas) return;

    auto bindText = [&](const char* name, UIText*& out) {
        if (UIElement* element = ScriptApi_UICanvasGetElement(*canvas, name))
            out = dynamic_cast<UIText*>(element);
    };
    auto bindSprite = [&](const char* name, UISprite*& out) {
        if (UIElement* element = ScriptApi_UICanvasGetElement(*canvas, name))
            out = dynamic_cast<UISprite*>(element);
    };

    bindText("ScoreText", m_scoreText);
    bindText("TimerText", m_timerText);
    bindSprite("ScoreBarBg", m_scoreBarBg);
    bindSprite("StatusPanelBg", m_statusPanelBg);
    bindSprite("StatusPanelBorder", m_statusPanelBorder);

    if (GameMode_Get() == GameMode::Team2v2) {
        bindSprite("ScoreBarBlue", m_scoreBars[0]);
        bindSprite("ScoreBarRed", m_scoreBars[1]);
        bindText("ScoreBlueText", m_scoreTexts[0]);
        bindText("ScoreRedText", m_scoreTexts[1]);
        m_scoreBars[2] = nullptr;
        m_scoreBars[3] = nullptr;
        m_scoreTexts[2] = nullptr;
        m_scoreTexts[3] = nullptr;
    } else {
        for (int i = 0; i < kGameScoreMaxPlayers; ++i) {
            char name[32];
            snprintf(name, sizeof(name), "ScoreBarP%d", i + 1);
            bindSprite(name, m_scoreBars[i]);
            snprintf(name, sizeof(name), "ScoreP%dText", i + 1);
            bindText(name, m_scoreTexts[i]);
        }
    }

    for (int i = 0; i < kGameScoreMaxPlayers; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "HudP%dIcon", i + 1);
        bindSprite(name, m_hudPlayerIcons[i]);
        snprintf(name, sizeof(name), "HudP%dEnergyBar", i + 1);
        bindSprite(name, m_hudPlayerEnergyBars[i]);
        snprintf(name, sizeof(name), "HudP%dName", i + 1);
        bindText(name, m_hudPlayerNames[i]);
        snprintf(name, sizeof(name), "HudP%dStatus", i + 1);
        bindText(name, m_hudPlayerStatuses[i]);
    }

    ApplyHudPlayerColorTextures();
}

void RoundManagerScript::BindResultOverlayElements()
{
    UICanvas* canvas = ScriptApi_SceneCanvasGetCanvas();
    if (!canvas) return;

    auto bindSprite = [&](const char* name, UISprite*& out) {
        if (UIElement* element = ScriptApi_UICanvasGetElement(*canvas, name))
            out = dynamic_cast<UISprite*>(element);
    };
    auto bindText = [&](const char* name, UIText*& out) {
        if (UIElement* element = ScriptApi_UICanvasGetElement(*canvas, name))
            out = dynamic_cast<UIText*>(element);
    };

    for (int i = 0; i < kGameScoreMaxPlayers; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "p%d result", i + 1);
        bindSprite(name, m_resultPanels[i]);
        snprintf(name, sizeof(name), "p%d score", i + 1);
        bindText(name, m_resultScores[i]);
        snprintf(name, sizeof(name), "p%d death", i + 1);
        bindText(name, m_resultDeaths[i]);
    }

    if (UIElement* element = ScriptApi_UICanvasGetElement(*canvas, "back"))
        m_backButton = dynamic_cast<UIButton*>(element);
}

void RoundManagerScript::RefreshResultOverlay()
{
    for (int i = 0; i < kGameScoreMaxPlayers; ++i) {
        char buffer[16];
        if (m_resultScores[i]) {
            snprintf(buffer, sizeof(buffer), "%03d", GameScore_Get(i));
            ScriptApi_UITextSetText(*m_resultScores[i], buffer);
        }
        if (m_resultDeaths[i]) {
            snprintf(buffer, sizeof(buffer), "%d", GameScore_GetDeaths(i));
            ScriptApi_UITextSetText(*m_resultDeaths[i], buffer);
        }
    }

    if (GameMode_Get() == GameMode::Team2v2 && m_scoreText) {
        const int blue = GameScore_Get(0) + GameScore_Get(1);
        const int red = GameScore_Get(2) + GameScore_Get(3);
        const char* message = "DRAW";
        if (blue > red)
            message = "BLUE WINS";
        else if (red > blue)
            message = "RED WINS";
        ScriptApi_UITextSetText(*m_scoreText, message);
        ScriptApi_UIElementSetVisible(*m_scoreText, true);
    }
}

void RoundManagerScript::ShowResultOverlay()
{
    if (!m_resultPanels[0])
        BindResultOverlayElements();

    RefreshResultOverlay();

    for (int i = 0; i < kGameScoreMaxPlayers; ++i) {
        if (m_resultPanels[i])
            ScriptApi_UIElementSetVisible(*m_resultPanels[i], true);
        if (m_resultScores[i])
            ScriptApi_UIElementSetVisible(*m_resultScores[i], true);
        if (m_resultDeaths[i])
            ScriptApi_UIElementSetVisible(*m_resultDeaths[i], true);
    }

    if (m_backButton)
        ScriptApi_UIElementSetVisible(*m_backButton, true);
}

void RoundManagerScript::HideResultOverlay()
{
    for (int i = 0; i < kGameScoreMaxPlayers; ++i) {
        if (m_resultPanels[i])
            ScriptApi_UIElementSetVisible(*m_resultPanels[i], false);
        if (m_resultScores[i])
            ScriptApi_UIElementSetVisible(*m_resultScores[i], false);
        if (m_resultDeaths[i])
            ScriptApi_UIElementSetVisible(*m_resultDeaths[i], false);
    }

    if (m_backButton)
        ScriptApi_UIElementSetVisible(*m_backButton, false);
}

bool RoundManagerScript::IsHudPlayerAlive(int playerIndex) const
{
    if (playerIndex == 0) {
        if (PlayerScript_IsRespawning() || m_playerRespawnTimer >= 0.0f)
            return false;
        if (GameObject* body = PlayerScript_GetBody()) {
            if (body->GetPosition().y < kFallThresholdY)
                return false;
        }
        return true;
    }

    if (playerIndex > 0 && playerIndex < kGameScoreMaxPlayers) {
        const std::vector<EnemyAiScript*>& scripts = EnemyAi_GetActiveScripts();
        EnemyAiScript* ai = nullptr;
        for (EnemyAiScript* candidate : scripts) {
            if (candidate && candidate->GetPlayerIndex() == playerIndex) {
                ai = candidate;
                break;
            }
        }
        if (!ai)
            return false;

        if (EnemyAi_IsRespawning(ai))
            return false;

        if (GameObject* proxy = EnemyAi_GetPhysicsProxy(ai)) {
            if (proxy->GetPosition().y < kFallThresholdY)
                return false;
        }

        for (const EnemyRespawnEntry& entry : m_enemyRespawns) {
            if (entry.script == ai && entry.timer > 0.0f)
                return false;
        }
        return true;
    }

    return true;
}

void RoundManagerScript::OnDestroy(engine::GameObject& self)
{
    (void)self;
    if (g_activeRoundManager == this)
        g_activeRoundManager = nullptr;
    IntroCameraScript::End();
    CancelAllRespawns();
    m_floorObject = nullptr;
    m_scoreText = nullptr;
    m_timerText = nullptr;
    m_scoreBarBg = nullptr;
    m_statusPanelBg = nullptr;
    m_statusPanelBorder = nullptr;
    for (int i = 0; i < kGameScoreMaxPlayers; ++i) {
        m_scoreBars[i] = nullptr;
        m_scoreTexts[i] = nullptr;
        m_hudPlayerIcons[i] = nullptr;
        m_hudPlayerEnergyBars[i] = nullptr;
        m_hudPlayerNames[i] = nullptr;
        m_hudPlayerStatuses[i] = nullptr;
        m_resultPanels[i] = nullptr;
        m_resultScores[i] = nullptr;
        m_resultDeaths[i] = nullptr;
    }
    m_backButton = nullptr;
    m_wasPlaying = false;
    m_isPreStartActive = false;
    m_isGameFinished = false;
    m_preStartTimer = 0.0f;
    m_goFlashTimer = -1.0f;
    m_isShrinkAnimating = false;
}

void RoundManagerScript::EnsureFloorObject()
{
    SceneBase* scene = ScriptApi_GetCurrentScene();
    if (!scene) return;

    float bestArea = -1.0f;
    GameObject* bestFloor = nullptr;
    DirectX::XMFLOAT3 bestColliderSize = { 1.0f, 1.0f, 1.0f };

    for (GameObject* obj : scene->GetObjects()) {
        if (!obj || !obj->IsActive() || obj->GetTag() != Tag::TAG_GROUND) continue;

        Collider* collider = obj->GetCollider();
        if (!collider) continue;

        DirectX::XMFLOAT3 center = {};
        DirectX::XMFLOAT3 size = {1.0f, 1.0f, 1.0f};
        if (!ScriptApi_TryGetColliderFootprint(*collider, center, size))
            continue;

        const float area = size.x * size.z;
        if (area > bestArea) {
            bestArea = area;
            bestFloor = obj;
            bestColliderSize = size;
        }
    }

    if (!bestFloor) return;

    const bool floorChanged = (bestFloor != m_floorObject);
    m_floorObject = bestFloor;
    if (floorChanged || !m_floorBaselineCaptured) {
        m_initialFloorScale = GetFloorVisualScale();
        m_initialFloorColliderSize = bestColliderSize;
        m_floorBaselineCaptured = true;
    }
}

void RoundManagerScript::ResetFloorSize()
{
    if (!m_floorObject) return;

    ApplyFloorVisualScale(m_initialFloorScale);
    ApplyFloorColliderSize(m_floorObject, m_initialFloorColliderSize);

    m_isShrinkAnimating = false;
    m_shrinkAnimElapsed = 0.0f;
}

void RoundManagerScript::StartFloorShrinkAnimation(float targetScaleXZ)
{
    if (!m_floorObject) return;

    m_shrinkAnimStartScale = GetFloorVisualScale();
    m_shrinkAnimStartColliderSize = GetFloorColliderSize(m_floorObject, m_initialFloorColliderSize);

    m_shrinkAnimTargetScale = m_initialFloorScale;
    m_shrinkAnimTargetScale.x *= targetScaleXZ;
    m_shrinkAnimTargetScale.z *= targetScaleXZ;

    m_shrinkAnimTargetColliderSize = m_initialFloorColliderSize;
    m_shrinkAnimTargetColliderSize.x *= targetScaleXZ;
    m_shrinkAnimTargetColliderSize.z *= targetScaleXZ;

    m_shrinkAnimElapsed = 0.0f;
    m_isShrinkAnimating = true;
}

void RoundManagerScript::UpdateFloorShrinkAnimation(float deltaSeconds)
{
    if (!m_isShrinkAnimating || !m_floorObject) return;

    m_shrinkAnimElapsed += (std::max)(0.0f, deltaSeconds);
    const float t = (std::min)(1.0f, m_shrinkAnimElapsed / kShrinkAnimDurationSeconds);

    DirectX::XMFLOAT3 newScale = {
        Lerp(m_shrinkAnimStartScale.x, m_shrinkAnimTargetScale.x, t),
        Lerp(m_shrinkAnimStartScale.y, m_shrinkAnimTargetScale.y, t),
        Lerp(m_shrinkAnimStartScale.z, m_shrinkAnimTargetScale.z, t)
    };
    ApplyFloorVisualScale(newScale);

    DirectX::XMFLOAT3 newColliderSize = {
        Lerp(m_shrinkAnimStartColliderSize.x, m_shrinkAnimTargetColliderSize.x, t),
        Lerp(m_shrinkAnimStartColliderSize.y, m_shrinkAnimTargetColliderSize.y, t),
        Lerp(m_shrinkAnimStartColliderSize.z, m_shrinkAnimTargetColliderSize.z, t)
    };
    ApplyFloorColliderSize(m_floorObject, newColliderSize);

    if (t >= 1.0f) {
        m_isShrinkAnimating = false;
    }
}

bool RoundManagerScript::TryPickRandomRespawnPosition(XMFLOAT3& outPosition)
{
    EnsureFloorObject();

    outPosition.y = kRespawnHoldY;
    if (m_floorObject && m_floorObject->GetCollider()) {
        XMFLOAT3 floorCenter = {0.0f, 0.0f, 0.0f};
        XMFLOAT3 floorSize = {20.0f, 1.0f, 20.0f};
        if (!ScriptApi_TryGetColliderFootprint(*m_floorObject->GetCollider(), floorCenter, floorSize))
            floorCenter = m_floorObject->GetPosition();

        const float angle = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 6.2831853f;
        const float radius = kSpawnRadiusFromCenter;

        outPosition = {
            floorCenter.x + radius * cosf(angle),
            kRespawnHoldY,
            floorCenter.z + radius * sinf(angle)
        };
        return true;
    }

    return RoundManager_TryPickRandomSpawnPosition(outPosition);
}

void RoundManagerScript::CancelAllRespawns()
{
    if (m_playerRespawnTimer >= 0.0f) {
        PlayerScript_EndRespawn();
        SceneCanvas_HideCountdown();
        SceneCanvas_HideRedOverlay();
        EngineGUI_SetCameraMode(CameraMode::ThirdPerson);
        m_playerRespawnTimer = -1.0f;
    }

    for (const EnemyRespawnEntry& entry : m_enemyRespawns) {
        if (entry.script)
            EnemyAi_EndRespawn(entry.script);
    }
    m_enemyRespawns.clear();
}

void RoundManagerScript::UpdateRespawns(float deltaSeconds)
{
    const bool trainingMode = GameMode_IsTraining();

    GameObject* playerBody = PlayerScript_GetBody();
    if (playerBody && m_playerRespawnTimer < 0.0f &&
        playerBody->GetPosition().y < kFallThresholdY) {
        XMFLOAT3 holdPos = {0.0f, kRespawnHoldY, 0.0f};
        if (trainingMode) {
            XMFLOAT3 spawnPos = {};
            if (PlayerScript_TryGetSpawnPosition(spawnPos)) {
                holdPos = { spawnPos.x, kRespawnHoldY, spawnPos.z };
            } else {
                holdPos = { playerBody->GetPosition().x, kRespawnHoldY, playerBody->GetPosition().z };
            }
        } else {
            TryPickRandomRespawnPosition(holdPos);
        }
        m_playerRespawnTimer = kRespawnCountdownDuration;
        PlayerScript_BeginRespawnAt(holdPos);
        GameScore_AddDeath(0);
        SceneCanvas_ShowRedOverlay();
        ScriptApi_ActivateSceneCamera("DeathCam");
    }

    if (m_playerRespawnTimer >= 0.0f) {
        m_playerRespawnTimer -= deltaSeconds;

        const float t = m_playerRespawnTimer / kRespawnCountdownDuration;
        const float alpha = kMinRespawnOverlayAlpha + t * (kMaxRespawnOverlayAlpha - kMinRespawnOverlayAlpha);
        SceneCanvas_SetRedOverlayAlpha(alpha);

        const int countdown = static_cast<int>(ceilf(m_playerRespawnTimer));
        if (countdown > 0)
            SceneCanvas_ShowCountdown(countdown);

        PlayerScript_TickRespawnHold();

        if (m_playerRespawnTimer <= 0.0f) {
            PlayerScript_EndRespawn();
            SceneCanvas_HideCountdown();
            SceneCanvas_HideRedOverlay();
            EngineGUI_SetCameraMode(CameraMode::ThirdPerson);
            m_playerRespawnTimer = -1.0f;
        }
    }

    for (EnemyAiScript* ai : EnemyAi_GetActiveScripts()) {
        if (!ai || EnemyAi_IsRespawning(ai)) continue;

        GameObject* proxy = EnemyAi_GetPhysicsProxy(ai);
        if (!proxy || proxy->GetPosition().y >= kFallThresholdY) continue;

        XMFLOAT3 holdPos = {0.0f, kRespawnHoldY, 0.0f};
        float respawnDelay = kRespawnCountdownDuration;
        if (trainingMode) {
            XMFLOAT3 spawnPos = {};
            if (EnemyAi_TryGetSpawnPosition(ai, spawnPos))
                holdPos = spawnPos;
            else
                holdPos = proxy->GetPosition();
            respawnDelay = 0.0f;
        } else {
            TryPickRandomRespawnPosition(holdPos);
        }
        EnemyAi_BeginRespawnAt(ai, holdPos);
        GameScore_AddDeath(ai->GetPlayerIndex());
        m_enemyRespawns.push_back({ ai, respawnDelay });
    }

    for (auto it = m_enemyRespawns.begin(); it != m_enemyRespawns.end(); ) {
        if (!it->script) {
            it = m_enemyRespawns.erase(it);
            continue;
        }

        it->timer -= deltaSeconds;
        EnemyAi_TickRespawnHold(it->script);

        if (it->timer <= 0.0f) {
            EnemyAi_EndRespawn(it->script);
            it = m_enemyRespawns.erase(it);
        } else {
            ++it;
        }
    }
}

void RoundManagerScript::BeginPreStartPresentation()
{
    IntroCameraScript::Begin();
    ThirdPersonCamera_ShowCursor(true);
}

void RoundManagerScript::EndPreStartPresentation()
{
    IntroCameraScript::End();
    EngineGUI_SetCameraMode(CameraMode::ThirdPerson);
    ThirdPersonCamera_ShowCursor(false);
}

void RoundManagerScript::UpdatePreStartCountdown(float deltaSeconds)
{
    if (m_goFlashTimer >= 0.0f) {
        m_goFlashTimer -= deltaSeconds;
        if (m_goFlashTimer <= 0.0f) {
            SceneCanvas_HideCountdown();
            m_isPreStartActive = false;
            m_goFlashTimer = -1.0f;
            EndPreStartPresentation();
        }
        return;
    }

    m_preStartTimer -= deltaSeconds;
    const int countdown = static_cast<int>(ceilf(m_preStartTimer));
    if (countdown > 0) {
        SceneCanvas_ShowCountdown(countdown);
        return;
    }

    SceneCanvas_ShowCountdownText("GO!");
    m_goFlashTimer = kGoFlashDurationSeconds;
}

void RoundManagerScript::StartRound()
{
    EnsureFloorObject();
    ResetFloorSize();
    CancelAllRespawns();
    GameScore_Reset();
    HideResultOverlay();

    m_remainingTimeSeconds = gameDurationSeconds;
    m_shrinkAnimElapsed = 0.0f;
    m_isGameFinished = false;
    m_isShrinkAnimating = false;
    m_shrunkAt50Percent = false;
    m_shrunkAt75Percent = false;
    m_isPreStartActive = true;
    m_preStartTimer = kPreStartCountdownSeconds;
    m_goFlashTimer = -1.0f;
    BeginPreStartPresentation();
    SceneCanvas_ShowCountdown(static_cast<int>(kPreStartCountdownSeconds));
    RefreshHud();
}

void RoundManagerScript::EndRound()
{
    if (m_isGameFinished) return;
    m_isGameFinished = true;
    m_remainingTimeSeconds = 0.0f;
    CancelAllRespawns();
    RefreshHud();

    ThirdPersonCamera_ShowCursor(true);
    ThirdPersonCamera_SetTarget(nullptr, nullptr);

    ShowResultOverlay();
}

void RoundManagerScript::RefreshHud()
{
    if (!m_timerText || !m_scoreBarBg)
        BindHudElements();

    ApplyHudPlayerColorTextures();

    const bool trainingMode = GameMode_IsTraining();
    if (m_timerText)
        ScriptApi_UIElementSetVisible(*m_timerText, !trainingMode);
    if (!trainingMode)
        ScriptApi_SceneCanvasSetTimerSeconds(m_remainingTimeSeconds);

    const bool teamMode = GameMode_Get() == GameMode::Team2v2;
    const int barCount = teamMode ? 2 : kGameScoreMaxPlayers;

    int scores[kGameScoreMaxPlayers] = {};
    int totalScore = 0;
    if (teamMode) {
        scores[0] = GameScore_Get(0) + GameScore_Get(1);
        scores[1] = GameScore_Get(2) + GameScore_Get(3);
        totalScore = scores[0] + scores[1];
    } else {
        for (int i = 0; i < kGameScoreMaxPlayers; ++i) {
            scores[i] = GameScore_Get(i);
            totalScore += scores[i];
        }
    }

    float widths[kGameScoreMaxPlayers] = {};
    if (totalScore > 0) {
        float usedWidth = 0.0f;
        for (int i = 0; i < barCount; ++i) {
            widths[i] = kScoreBarTotalWidth * (static_cast<float>(scores[i]) / static_cast<float>(totalScore));
            if (scores[i] > 0 && widths[i] < 8.0f)
                widths[i] = 8.0f;
            usedWidth += widths[i];
        }
        if (usedWidth > kScoreBarTotalWidth && usedWidth > 0.001f) {
            const float scale = kScoreBarTotalWidth / usedWidth;
            for (int i = 0; i < barCount; ++i)
                widths[i] *= scale;
        }
    } else {
        for (int i = 0; i < barCount; ++i)
            widths[i] = kScoreBarTotalWidth / static_cast<float>(barCount);
    }

    const float barLeftX = -kScoreBarTotalWidth * 0.5f;
    if (m_scoreBarBg) {
        m_scoreBarBg->SetSize(kScoreBarTotalWidth, kScoreBarHeight);
        m_scoreBarBg->SetPosition({ barLeftX, kScoreBarTopY });
    }

    float cursorX = barLeftX;
    for (int i = 0; i < barCount; ++i) {
        if (m_scoreBars[i]) {
            m_scoreBars[i]->SetSize(widths[i], kScoreBarHeight);
            m_scoreBars[i]->SetPosition({ cursorX, kScoreBarTopY });
        }
        if (m_scoreTexts[i]) {
            ScriptApi_UITextSetText(*m_scoreTexts[i], std::to_string(scores[i]).c_str());
            m_scoreTexts[i]->SetPosition({ cursorX + widths[i] * 0.5f, kScoreBarTopY + kScoreBarHeight + 6.0f });
        }
        cursorX += widths[i];
    }

    if (m_scoreText && !m_isGameFinished) {
        if (teamMode)
            ScriptApi_UITextSetText(*m_scoreText,
                ("Blue: " + std::to_string(scores[0]) + "  Red: " + std::to_string(scores[1])).c_str());
        else
            ScriptApi_UITextSetText(*m_scoreText, ("Score: " + std::to_string(GameScore_Get(0))).c_str());
    }

    static const DirectX::XMFLOAT4 kAliveTint = {1.0f, 1.0f, 1.0f, 1.0f};
    static const DirectX::XMFLOAT4 kDeadTint = {0.45f, 0.45f, 0.45f, 1.0f};
    for (int i = 0; i < kGameScoreMaxPlayers; ++i) {
        const bool alive = IsHudPlayerAlive(i);
        if (m_hudPlayerIcons[i])
            m_hudPlayerIcons[i]->SetColor(alive ? kAliveTint : kDeadTint);
        if (m_hudPlayerEnergyBars[i])
            m_hudPlayerEnergyBars[i]->SetColor(alive ? kAliveTint : kDeadTint);
        if (m_hudPlayerStatuses[i])
            ScriptApi_UITextSetText(*m_hudPlayerStatuses[i], alive ? "Alive" : "Dead");
    }
}

void RoundManagerScript::OnUpdate(engine::GameObject& self, float dt)
{
    (void)self;

    const bool isPlaying = EngineGUI_IsPlaying();

    if (isPlaying && !m_wasPlaying) {
        StartRound();
    }
    if (!isPlaying && m_wasPlaying) {
        CancelAllRespawns();
        ResetFloorSize();
        m_isPreStartActive = false;
        m_isGameFinished = false;
        m_goFlashTimer = -1.0f;
        SceneCanvas_HideCountdown();
        HideResultOverlay();
        IntroCameraScript::End();
        EngineGUI_SetCameraMode(CameraMode::ThirdPerson);
    }

    m_wasPlaying = isPlaying;
    if (!isPlaying || m_isGameFinished) return;

    if (m_isPreStartActive) {
        UpdatePreStartCountdown(dt);
        return;
    }

    UpdateRespawns(dt);

    const bool trainingMode = GameMode_IsTraining();
    if (!trainingMode) {
        m_remainingTimeSeconds = (std::max)(0.0f, m_remainingTimeSeconds - dt);
        const float duration = (std::max)(0.001f, gameDurationSeconds);
        const float elapsedRatio = 1.0f - (m_remainingTimeSeconds / duration);

        if (!m_shrunkAt50Percent && elapsedRatio >= 0.50f) {
            StartFloorShrinkAnimation(kFloorScaleAt50PercentElapsed);
            m_shrunkAt50Percent = true;
        }
        else if (!m_shrunkAt75Percent && elapsedRatio >= 0.75f) {
            StartFloorShrinkAnimation(kFloorScaleAt75PercentElapsed);
            m_shrunkAt75Percent = true;
        }

        UpdateFloorShrinkAnimation(dt);
    }

    RefreshHud();

    if (!trainingMode && m_remainingTimeSeconds <= 0.0f) {
        EndRound();
    }
}
