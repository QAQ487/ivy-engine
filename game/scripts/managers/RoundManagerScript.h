#pragma once

#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"
#include "game_score.h"

#include <vector>

class GameObject;
class UIText;
class UISprite;
class UIButton;

bool RoundManager_IsGameplayActive();
bool RoundManager_TryPickRandomSpawnPosition(DirectX::XMFLOAT3& outPosition);
void RoundManager_EnsureMinSpawnRadius(DirectX::XMFLOAT3& inOutPosition);
bool RoundManager_IsPlayerEligibleForScoring(int playerIndex);

inline constexpr float kSpawnRadiusFromCenter = 5.0f;

class RoundManagerScript : public Script {
#define ROUND_MANAGER_SCRIPT_FIELDS(X) \
    X(float, gameDurationSeconds, 60.0f)

public:
    ROUND_MANAGER_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;
    void OnDestroy(engine::GameObject& self) override;

    bool IsGameplayActive() const { return !m_isPreStartActive && !m_isGameFinished; }
    bool IsPlayerEligibleForScoring(int playerIndex) const {
        return IsHudPlayerAlive(playerIndex);
    }

    SCRIPT_SERIALIZE_METHODS(ROUND_MANAGER_SCRIPT_FIELDS)

private:
    static constexpr float kShrinkAnimDurationSeconds = 1.0f;
    static constexpr float kFloorScaleAt50PercentElapsed = 0.75f;
    static constexpr float kFloorScaleAt75PercentElapsed = 0.50f;
    static constexpr float kFallThresholdY = 0.0f;
    static constexpr float kPreStartCountdownSeconds = 3.0f;
    static constexpr float kGoFlashDurationSeconds = 0.8f;
    static constexpr float kRespawnCountdownDuration = 5.0f;
    static constexpr float kRespawnHoldY = 5.0f;
    static constexpr float kRespawnEdgePadding = 1.5f;
    static constexpr float kMaxRespawnOverlayAlpha = 0.8f;
    static constexpr float kMinRespawnOverlayAlpha = 0.3f;

    void StartRound();
    void EndRound();
    void EnsureFloorObject();
    void ResetFloorSize();
    void StartFloorShrinkAnimation(float targetScaleXZ);
    void UpdateFloorShrinkAnimation(float deltaSeconds);
    void RefreshHud();
    void BindHudElements();
    void BindResultOverlayElements();
    void ShowResultOverlay();
    void HideResultOverlay();
    void RefreshResultOverlay();
    void ApplyHudPlayerColorTextures();
    bool IsHudPlayerAlive(int playerIndex) const;
    void ApplyFloorVisualScale(const DirectX::XMFLOAT3& scale);
    DirectX::XMFLOAT3 GetFloorVisualScale() const;
    void UpdateRespawns(float deltaSeconds);
    void UpdatePreStartCountdown(float deltaSeconds);
    void BeginPreStartPresentation();
    void EndPreStartPresentation();
    void CancelAllRespawns();
    bool TryPickRandomRespawnPosition(DirectX::XMFLOAT3& outPosition);

    GameObject* m_floorObject = nullptr;
    DirectX::XMFLOAT3 m_initialFloorScale = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 m_initialFloorColliderSize = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 m_shrinkAnimStartScale = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 m_shrinkAnimStartColliderSize = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 m_shrinkAnimTargetScale = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 m_shrinkAnimTargetColliderSize = { 1.0f, 1.0f, 1.0f };
    float m_remainingTimeSeconds = 60.0f;
    float m_shrinkAnimElapsed = 0.0f;
    bool m_wasPlaying = false;
    bool m_isPreStartActive = false;
    bool m_isGameFinished = false;
    float m_preStartTimer = 0.0f;
    float m_goFlashTimer = -1.0f;
    bool m_floorBaselineCaptured = false;
    bool m_isShrinkAnimating = false;
    bool m_shrunkAt50Percent = false;
    bool m_shrunkAt75Percent = false;
    UIText* m_scoreText = nullptr;
    UIText* m_timerText = nullptr;
    UISprite* m_scoreBarBg = nullptr;
    UISprite* m_scoreBars[kGameScoreMaxPlayers] = {};
    UIText* m_scoreTexts[kGameScoreMaxPlayers] = {};
    UISprite* m_statusPanelBg = nullptr;
    UISprite* m_statusPanelBorder = nullptr;
    UISprite* m_hudPlayerIcons[kGameScoreMaxPlayers] = {};
    UISprite* m_hudPlayerEnergyBars[kGameScoreMaxPlayers] = {};
    UIText* m_hudPlayerNames[kGameScoreMaxPlayers] = {};
    UIText* m_hudPlayerStatuses[kGameScoreMaxPlayers] = {};
    UISprite* m_resultPanels[kGameScoreMaxPlayers] = {};
    UIText* m_resultScores[kGameScoreMaxPlayers] = {};
    UIText* m_resultDeaths[kGameScoreMaxPlayers] = {};
    UIButton* m_backButton = nullptr;
    static constexpr float kScoreBarTotalWidth = 960.0f;
    static constexpr float kScoreBarHeight = 28.0f;
    static constexpr float kScoreBarTopY = 18.0f;
    float m_playerRespawnTimer = -1.0f;
    struct EnemyRespawnEntry {
        class EnemyAiScript* script = nullptr;
        float timer = -1.0f;
    };
    std::vector<EnemyRespawnEntry> m_enemyRespawns;
};
