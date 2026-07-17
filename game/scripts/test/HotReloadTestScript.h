#pragma once

#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"

class HotReloadTestScript : public Script {
#define HOT_RELOAD_TEST_SCRIPT_FIELDS(X) \
    X(float, riseSpeed, 0.5f)

public:
    HOT_RELOAD_TEST_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

    void OnUpdate(engine::GameObject& self, float dt) override;

    SCRIPT_SERIALIZE_METHODS(HOT_RELOAD_TEST_SCRIPT_FIELDS)
};
