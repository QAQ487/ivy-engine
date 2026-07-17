#pragma once
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"

// Teaching script: spins an object around Y.
class SampleSpinScript : public Script {
public:
#define SAMPLE_SPIN_FIELDS(X) \
    X(float, degreesPerSecond, 45.0f)

    SAMPLE_SPIN_FIELDS(SCRIPT_DECLARE_FIELD)
    SCRIPT_SERIALIZE_METHODS(SAMPLE_SPIN_FIELDS)

    void OnUpdate(engine::GameObject& self, float dt) override;
};
