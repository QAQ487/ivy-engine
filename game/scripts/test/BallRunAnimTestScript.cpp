#include "BallRunAnimTestScript.h"

#include "engine/ecs/visual_component.h"
#include "engine/input/key_logger.h"
#include "engine/input/keyboard.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"

REGISTER_SCRIPT(BallRunAnimTestScript)

void BallRunAnimTestScript::OnUpdate(engine::GameObject& self, float /*dt*/) {
    VisualComponent* vc = ScriptApi_FindVisualComponent(self);
    if (!vc || !vc->playAnimation) return;

    if (KeyLogger_IsTrigger(KK_D1))
        ScriptApi_Visual_PlayClip(vc, runClip.c_str());
    else if (KeyLogger_IsTrigger(KK_D2))
        ScriptApi_Visual_PlayClip(vc, danceClip.c_str());
    else if (KeyLogger_IsTrigger(KK_D3))
        ScriptApi_Visual_PlayClip(vc, jumpClip.c_str());
    else if(KeyLogger_IsTrigger(KK_D4))
        ScriptApi_Visual_PlayClip(vc, pushClip.c_str());
    else if (KeyLogger_IsTrigger(KK_D5))
        ScriptApi_Visual_PlayClip(vc, pullClip.c_str());
    else if (KeyLogger_IsTrigger(KK_D6))
		ScriptApi_Visual_PlayClip(vc, throwClip.c_str());
}
