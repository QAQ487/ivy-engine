#pragma once

#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"
#include <DirectXMath.h>

namespace engine { class Transform; }

class LeaderSpotlightScript : public Script {
#define LEADER_SPOTLIGHT_SCRIPT_FIELDS(X) \
    X(float, rigHeight,        22.0f) \
    X(float, aimHeightOffset,   1.5f) \
    X(int,   teamSlot,          0)

public:
    LEADER_SPOTLIGHT_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;

    SCRIPT_SERIALIZE_METHODS(LEADER_SPOTLIGHT_SCRIPT_FIELDS)

private:
    engine::Transform* m_transform = nullptr;
};
