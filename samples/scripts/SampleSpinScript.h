// SampleSpinScript.h
// Declares SampleSpinScript.

#pragma once
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"

// Teaching / job-timeline demo: spins an object around Y (parallel-safe).
class SampleSpinScript : public Script {
public:
#define SAMPLE_SPIN_FIELDS(X) \
    X(float, degreesPerSecond, 45.0f) \
    X(float, busyMillis, 0.0f)

    SAMPLE_SPIN_FIELDS(SCRIPT_DECLARE_FIELD)
    SCRIPT_SERIALIZE_METHODS(SAMPLE_SPIN_FIELDS)

    void OnUpdate(engine::GameObject& self, float dt) override;
    bool Update_IsParallelSafe() const override;
};
