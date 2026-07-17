#include "EffectReleaseTestScript.h"

#include "engine/ecs/effect_component.h"
#include "engine/input/key_logger.h"
#include "engine/input/keyboard.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"

REGISTER_SCRIPT(EffectReleaseTestScript)

void EffectReleaseTestScript::OnUpdate(engine::GameObject& self, float /*dt*/) {
    if (!KeyLogger_IsTrigger(KK_SPACE)) return;

    EffectComponent* effect = ScriptApi_FindEffectComponent(self);
    if (effect)
        ScriptApi_Effect_Play(effect);
}
