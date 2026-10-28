#pragma once
#include "SampleCheckpointState.h"
#include "engine/scripting/Script.h"

// Debug-runtime checkpoint trigger: one-shot Player OnTriggerEnter -> score 1 + bridge event.
class SampleCheckpointScript : public Script {
public:
    void OnTriggerEnter(engine::GameObject& self, engine::GameObject* other) override;

private:
    SampleCheckpointLatch m_latch;
};
