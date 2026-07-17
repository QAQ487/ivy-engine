/**===========================================================================
 File   : OffscreenEnemyIndicator.cpp
 Brief  : Screen-edge arrows for offscreen score opponents.
============================================================================**/
#include "OffscreenEnemyIndicator.h"

#include "EnemyAiScript.h"
#include "PlayerColor.h"
#include "RoundManagerScript.h"
#include "game_score.h"

#include "engine/ecs/game_object.h"
#include "engine/scripting/script_runtime_api.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

int OffscreenEnemyIndicator::s_arrowTextureId = -1;

namespace {

constexpr float kArrowSize = 112.0f;
constexpr float kArrowAlpha = 0.7f;
constexpr float kEdgePadding = 8.0f;
constexpr float kAimHeightOffset = 1.0f;
constexpr float kRadToDeg = 180.0f / 3.14159265f;

bool ProjectWorldToScreen(const XMFLOAT3& world,
                          const XMMATRIX& viewProj,
                          float viewportW,
                          float viewportH,
                          float& outX,
                          float& outY,
                          bool& outBehind)
{
    const XMVECTOR clip = XMVector4Transform(
        XMVectorSet(world.x, world.y, world.z, 1.0f),
        viewProj);

    const float w = XMVectorGetW(clip);
    outBehind = (w <= 0.0f);

    float ndcX = XMVectorGetX(clip);
    float ndcY = XMVectorGetY(clip);
    if (std::fabs(w) > 1.0e-6f) {
        ndcX /= w;
        ndcY /= w;
    }

    if (outBehind) {
        ndcX = -ndcX;
        ndcY = -ndcY;
    }

    outX = (ndcX * 0.5f + 0.5f) * viewportW;
    outY = (1.0f - (ndcY * 0.5f + 0.5f)) * viewportH;
    return true;
}

bool ClampToInsetEdge(float screenX,
                      float screenY,
                      float viewportW,
                      float viewportH,
                      float margin,
                      float& outEdgeX,
                      float& outEdgeY)
{
    const float left = margin;
    const float right = viewportW - margin;
    const float top = margin;
    const float bottom = viewportH - margin;

    if (right <= left || bottom <= top)
        return false;

    // Axis-align clamp keeps the marker sliding continuously around corners
    // (avoids the ray-cast edge flip that can reverse the aim vector).
    outEdgeX = (std::max)(left, (std::min)(right, screenX));
    outEdgeY = (std::max)(top, (std::min)(bottom, screenY));
    return true;
}

bool IsInsideInset(float screenX, float screenY, float viewportW, float viewportH, float margin)
{
    return screenX >= margin
        && screenX <= viewportW - margin
        && screenY >= margin
        && screenY <= viewportH - margin;
}

} // namespace

bool OffscreenEnemyIndicator::Initialize()
{
    if (s_arrowTextureId < 0)
        s_arrowTextureId = ScriptApi_TextureLoad(L"rom/texture/UI/arrow.png", false);
    return s_arrowTextureId >= 0;
}

void OffscreenEnemyIndicator::Finalize()
{
    if (s_arrowTextureId >= 0) {
        ScriptApi_TextureRelease(s_arrowTextureId);
        s_arrowTextureId = -1;
    }
}

void OffscreenEnemyIndicator::Update(double /*elapsed_time*/)
{
}

void OffscreenEnemyIndicator::Reset()
{
}

void OffscreenEnemyIndicator::Draw()
{
    if (s_arrowTextureId < 0)
        return;
    if (!RoundManager_IsGameplayActive())
        return;

    const float viewportW = ScriptApi_GetRenderViewportWidth();
    const float viewportH = ScriptApi_GetRenderViewportHeight();
    if (viewportW <= 1.0f || viewportH <= 1.0f)
        return;

    XMFLOAT4X4 viewMat{};
    XMFLOAT4X4 projMat{};
    ScriptApi_GetCameraViewMatrix(&viewMat);
    ScriptApi_GetCameraProjMatrix(viewportW, viewportH, &projMat);

    const XMMATRIX viewProj = XMLoadFloat4x4(&viewMat) * XMLoadFloat4x4(&projMat);
    const float halfSize = kArrowSize * 0.5f;
    const float margin = halfSize + kEdgePadding;
    const unsigned int texW = ScriptApi_TextureGetWidth(s_arrowTextureId);
    const unsigned int texH = ScriptApi_TextureGetHeight(s_arrowTextureId);
    if (texW == 0 || texH == 0)
        return;

    for (EnemyAiScript* ai : EnemyAi_GetActiveScripts()) {
        if (!ai || EnemyAi_IsRespawning(ai))
            continue;

        const int playerIndex = ai->GetPlayerIndex();
        if (playerIndex <= 0 || playerIndex >= kGameScoreMaxPlayers)
            continue;
        if (!RoundManager_IsPlayerEligibleForScoring(playerIndex))
            continue;

        GameObject* proxy = EnemyAi_GetPhysicsProxy(ai);
        if (!proxy || !proxy->IsActive())
            continue;

        const XMFLOAT3 pos = proxy->GetPosition();
        const XMFLOAT3 aimPos = { pos.x, pos.y + kAimHeightOffset, pos.z };

        float screenX = 0.0f;
        float screenY = 0.0f;
        bool behind = false;
        if (!ProjectWorldToScreen(aimPos, viewProj, viewportW, viewportH, screenX, screenY, behind))
            continue;

        if (!behind && IsInsideInset(screenX, screenY, viewportW, viewportH, margin))
            continue;

        float edgeX = 0.0f;
        float edgeY = 0.0f;
        if (!ClampToInsetEdge(screenX, screenY, viewportW, viewportH, margin, edgeX, edgeY))
            continue;

        // Aim from screen center toward the projected enemy so the tip always
        // faces outward — never use edge→projection (that flips inward when the
        // marker sits between the center and the projected point).
        const float centerX = viewportW * 0.5f;
        const float centerY = viewportH * 0.5f;
        float aimDx = screenX - centerX;
        float aimDy = screenY - centerY;
        if (std::fabs(aimDx) < 1.0e-3f && std::fabs(aimDy) < 1.0e-3f) {
            aimDx = edgeX - centerX;
            aimDy = edgeY - centerY;
        }
        const float angleDeg = std::atan2(aimDx, -aimDy) * kRadToDeg;

        // Sprite_Draw origin is top-left; convert so the clamp point sits at the quad center.
        const float drawX = edgeX - halfSize;
        const float drawY = edgeY - halfSize;

        PlayerColor color = PlayerColor_GetEnemy();
        if (playerIndex >= 0 && playerIndex < PlayerColor_LobbySlotCount)
            color = PlayerColor_GetLobbySlot(playerIndex);
        XMFLOAT4 tint = PlayerColor_GetTint(color);
        tint.w = kArrowAlpha;

        ScriptApi_SpriteDraw(
            s_arrowTextureId,
            drawX,
            drawY,
            kArrowSize,
            kArrowSize,
            0,
            0,
            static_cast<int>(texW),
            static_cast<int>(texH),
            angleDeg,
            tint);
    }
}
