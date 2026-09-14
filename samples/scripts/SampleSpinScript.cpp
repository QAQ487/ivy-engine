#include "SampleSpinScript.h"

#include "engine/ecs/transform.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"

#include <Windows.h>

REGISTER_SCRIPT(SampleSpinScript)

void SampleSpinScript::OnUpdate(engine::GameObject& self, float dt) {
    DirectX::XMFLOAT3 rot = self.GetRotation();
    rot.y += degreesPerSecond * dt;
    self.SetRotation(rot);
    if (engine::Transform* t = ScriptApi_FindTransform(self))
        t->localRotation = rot;

    // Optional CPU burn so Job System timeline bars are visible in the
    // dedicated job-timeline test scene (busyMillis > 0). Own-state only.
    if (busyMillis > 0.0f) {
        LARGE_INTEGER freq{};
        LARGE_INTEGER start{};
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
        const LONGLONG target =
            start.QuadPart + static_cast<LONGLONG>(busyMillis * 0.001 * freq.QuadPart);
        LARGE_INTEGER now{};
        do {
            QueryPerformanceCounter(&now);
        } while (now.QuadPart < target);
    }
}

bool SampleSpinScript::Update_IsParallelSafe() const {
    // Audited: only mutates owning object's transform (+ local busy wait).
    // No singletons, AudioManager, PrefabRuntime, or neighbor reads.
    return true;
}
