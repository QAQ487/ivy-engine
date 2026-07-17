#include "LeaderSpotlightScript.h"

#include "EnemyAiScript.h"
#include "PlayerScript.h"
#include "GameMode.h"
#include "game_score.h"
#include "engine/core/game_object.h"
#include "engine/ecs/spot_light_component.h"
#include "engine/ecs/transform.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"

#include <cmath>
#include <vector>

using namespace DirectX;

REGISTER_SCRIPT(LeaderSpotlightScript)

namespace {

constexpr float kPi = 3.14159265f;
constexpr float kRadToDeg = 180.0f / kPi;
constexpr XMFLOAT3 kArenaFallbackTarget = { 0.0f, 1.0f, 0.0f };

void CollectTeamBodies(int teamId, std::vector<GameObject*>& out) {
    out.clear();
    if (teamId == 0) {
        if (GameObject* player = PlayerScript_GetBody()) {
            if (!PlayerScript_IsRespawning() && player->IsActive())
                out.push_back(player);
        }
    }

    for (EnemyAiScript* ai : EnemyAi_GetActiveScripts()) {
        if (!ai || EnemyAi_IsRespawning(ai))
            continue;
        if (ai->GetTeamId() != teamId)
            continue;
        GameObject* proxy = EnemyAi_GetPhysicsProxy(ai);
        if (!proxy || !proxy->IsActive())
            continue;
        out.push_back(proxy);
    }
}

GameObject* ResolveFfaLeaderBody() {
    int bestIndex = 0;
    int bestScore = GameScore_Get(0);
    for (int i = 1; i < kGameScoreMaxPlayers; ++i) {
        const int score = GameScore_Get(i);
        if (score > bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }

    if (bestIndex == 0) {
        if (GameObject* player = PlayerScript_GetBody())
            return player;
    }

    for (EnemyAiScript* ai : EnemyAi_GetActiveScripts()) {
        if (!ai || EnemyAi_IsRespawning(ai))
            continue;
        if (ai->GetPlayerIndex() != bestIndex)
            continue;
        if (GameObject* proxy = EnemyAi_GetPhysicsProxy(ai))
            return proxy;
    }

    if (GameObject* player = PlayerScript_GetBody())
        return player;
    return nullptr;
}

/** One body per spotlight. teamSlot picks which lead-team member (0 or 1). */
GameObject* ResolveSpotlightBody(int teamSlot) {
    if (GameMode_Get() == GameMode::Team2v2) {
        const int blueScore = GameScore_Get(0) + GameScore_Get(1);
        const int redScore = GameScore_Get(2) + GameScore_Get(3);
        const int leadTeam = (redScore > blueScore) ? 1 : 0;

        std::vector<GameObject*> bodies;
        CollectTeamBodies(leadTeam, bodies);
        if (bodies.empty())
            CollectTeamBodies(1 - leadTeam, bodies);

        if (teamSlot < 0 || teamSlot >= static_cast<int>(bodies.size()))
            return nullptr;
        return bodies[static_cast<size_t>(teamSlot)];
    }

    // FFA: only slot 0 tracks leader; extra spotlights stay off.
    if (teamSlot != 0)
        return nullptr;
    return ResolveFfaLeaderBody();
}

XMFLOAT3 GetAimPoint(GameObject* body, float aimHeightOffset) {
    if (!body)
        return kArenaFallbackTarget;

    const XMFLOAT3 pos = body->GetPosition();
    return { pos.x, pos.y + aimHeightOffset, pos.z };
}

void AimSpotAtTarget(engine::Transform& transform, const XMFLOAT3& lightPos, const XMFLOAT3& aimPoint) {
    const float dx = aimPoint.x - lightPos.x;
    const float dy = aimPoint.y - lightPos.y;
    const float dz = aimPoint.z - lightPos.z;

    const float horizLen = std::sqrt(dx * dx + dz * dz);
    if (horizLen < 0.001f) {
        transform.localRotation = { 90.0f, 0.0f, 0.0f };
        return;
    }

    const float yaw = std::atan2f(dx, dz) * kRadToDeg;
    const float pitch = std::atan2f(-dy, horizLen) * kRadToDeg;
    transform.localRotation = { pitch, yaw, 0.0f };
}

} // namespace

void LeaderSpotlightScript::OnCreate(engine::GameObject& self) {
    m_transform = ScriptApi_FindTransform(self);
}

void LeaderSpotlightScript::OnUpdate(engine::GameObject& self, float /*dt*/) {
    if (!m_transform)
        m_transform = ScriptApi_FindTransform(self);
    if (!m_transform)
        return;

    GameObject* body = ResolveSpotlightBody(teamSlot);
    if (SpotLightComponent* spot = self.GetComponent<SpotLightComponent>())
        spot->enabled = (body != nullptr);

    if (!body)
        return;

    const XMFLOAT3 aimPoint = GetAimPoint(body, aimHeightOffset);
    const XMFLOAT3 lightPos = { aimPoint.x, rigHeight, aimPoint.z };

    m_transform->localPosition = lightPos;
    AimSpotAtTarget(*m_transform, lightPos, aimPoint);
}
