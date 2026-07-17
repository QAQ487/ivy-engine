#pragma once

#include "engine/scripting/Script.h"

class EffectReleaseTestScript : public Script {
public:
    void OnUpdate(engine::GameObject& self, float dt) override;
};
