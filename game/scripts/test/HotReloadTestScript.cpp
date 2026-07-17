#include "HotReloadTestScript.h"
#include "engine/ecs/transform.h"
#include "engine/scripting/script_dll_register.h"
#include <string>

REGISTER_SCRIPT(HotReloadTestScript)

void HotReloadTestScript::OnUpdate(engine::GameObject& self, float dt) {
    const float delta = riseSpeed * dt;

    DirectX::XMFLOAT3* position = &self.m_position;
    for (const auto& kv : self.GetComponents()) {
        Component* component = kv.second.get();
        if (component && std::string(component->GetTypeName()) == "SceneTransform") {
            auto* transform = static_cast<engine::Transform*>(component);
            position = &transform->localPosition;
            break;
        }
    }

    position->z += delta;
    self.m_position = *position;
}
