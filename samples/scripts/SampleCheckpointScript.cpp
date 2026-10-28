#include "SampleCheckpointScript.h"

#include "engine/scripting/script_dll_register.h"

#if defined(_DEBUG) && defined(GAME_RUNTIME)
#include "engine/testing/runtime_test_bridge.h"
#endif

REGISTER_SCRIPT(SampleCheckpointScript)

void SampleCheckpointScript::OnTriggerEnter(engine::GameObject& self, engine::GameObject* other) {
    if (other == nullptr)
        return;
    const std::string& actor = other->GetName();
    if (!m_latch.TryAcceptPlayer(actor.c_str()))
        return;
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    ivy::testing::RuntimeTestEvents_RecordCheckpoint(self.GetName().c_str(), actor.c_str(), m_latch.score);
#else
    (void)self;
#endif
}
