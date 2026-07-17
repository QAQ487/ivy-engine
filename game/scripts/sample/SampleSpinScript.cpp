#include "SampleSpinScript.h"

#include "engine/ecs/transform.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"

REGISTER_SCRIPT(SampleSpinScript)

void SampleSpinScript::OnUpdate(engine::GameObject& self, float dt) {
    DirectX::XMFLOAT3 rot = self.GetRotation();
    rot.y += degreesPerSecond * dt;
    self.SetRotation(rot);
    if (engine::Transform* t = ScriptApi_FindTransform(self))
        t->localRotation = rot;
}
