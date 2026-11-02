// runtime_test_world_apply.cpp
// Applies validated runtime-bridge world ops on the engine main thread.

#include "src/app/runtime_test_world_apply.h"

#if defined(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE)
#include "engine/core/content_path.h"
#include "engine/core/engine_gui.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/rigidbody_component.h"
#include "engine/physics/core/rigidbody.h"
#include "engine/scene/scene_loader.h"
#include "engine/testing/runtime_test_scene_arg.h"
#include <DirectXMath.h>
#include <cmath>
#include <cstring>
#include <functional>

namespace {
using DirectX::XMMATRIX;
using DirectX::XMMatrixRotationQuaternion;
using DirectX::XMQuaternionNormalize;
using DirectX::XMVectorSet;

// Match Transform / observe ZYX Euler convention (engine/ecs/transform.cpp).
DirectX::XMFLOAT3 EulerZyxDegreesFromQuaternion(float qx, float qy, float qz, float qw) {
    const auto q = XMQuaternionNormalize(XMVectorSet(qx, qy, qz, qw));
    const XMMATRIX rot = XMMatrixRotationQuaternion(q);
    const float r00 = rot.r[0].m128_f32[0];
    const float r10 = rot.r[1].m128_f32[0];
    const float r20 = rot.r[2].m128_f32[0];
    const float r21 = rot.r[2].m128_f32[1];
    const float r22 = rot.r[2].m128_f32[2];
    const float r11 = rot.r[1].m128_f32[1];
    const float r12 = rot.r[1].m128_f32[2];
    const float sy = sqrtf(r00 * r00 + r10 * r10);
    float rotX, rotY, rotZ;
    if (sy > 1e-6f) {
        rotX = atan2f(r21, r22);
        rotY = atan2f(-r20, sy);
        rotZ = atan2f(r10, r00);
    } else {
        rotX = atan2f(-r12, r11);
        rotY = atan2f(-r20, sy);
        rotZ = 0.0f;
    }
    return {
        DirectX::XMConvertToDegrees(rotX),
        DirectX::XMConvertToDegrees(rotY),
        DirectX::XMConvertToDegrees(rotZ)
    };
}

GameObject* FindNamedObject(const std::string& name) {
    GameObject* found = nullptr;
    std::function<void(GameObject&)> walk = [&](GameObject& obj) {
        if (found) return;
        if (obj.GetName() == name) {
            found = &obj;
            return;
        }
        for (const auto& child : obj.GetChildren()) {
            if (child) walk(*child);
        }
    };
    for (const auto& node : SceneLoader_GetSceneNodes()) {
        if (node) walk(*node);
        if (found) break;
    }
    return found;
}

bool IsDynamicBody(GameObject& obj, RigidbodyComponent** outRc = nullptr) {
    // Non-const GameObject so GetComponent returns RigidbodyComponent* (mutable Body()).
    RigidbodyComponent* rc = obj.GetComponent<RigidbodyComponent>();
    if (!rc) return false;
    Rigidbody& body = rc->Body();
    if (body.IsKinematic() || body.GetMass() <= 0.0f) return false;
    if (outRc) *outRc = rc;
    return true;
}

void SyncPhysicsAfterTeleport(GameObject& obj, bool zeroVelocity) {
    obj.m_previousPosition = obj.GetPosition();
    if (auto* rc = obj.GetComponent<RigidbodyComponent>()) {
        Rigidbody& body = rc->Body();
        DirectX::XMFLOAT3 pos = obj.GetPosition();
        DirectX::XMFLOAT3 rot = obj.GetRotation();
        // dt==0 updates m_lastRotation then returns (see Rigidbody::Integrate).
        body.Integrate(pos, rot, 0.0);
        if (zeroVelocity) {
            body.ClearAllVelocity();
            body.ClearAllForces();
        }
    }
}
} // namespace

bool RuntimeTestWorld_Apply(const ivy::testing::WorldOpRequest& op, std::string& errorOut) {
    errorOut.clear();
    using ivy::testing::WorldOpKind;

    if (op.kind == WorldOpKind::ResetScene) {
        std::string path = op.scenePath;
        if (path.empty()) {
            path = SceneLoader_GetCurrentScenePath();
            std::string normalized, err;
            if (!ivy::testing::ValidateRuntimeTestSceneArg(path, &normalized, &err)) {
                errorOut = "invalid_scene";
                return false;
            }
            path = std::move(normalized);
        }
        if (content_path::ReadableFileCurrent(path).empty()) {
            errorOut = "scene_not_found";
            return false;
        }
        if (!EngineGUI_LoadFromFile(path.c_str())) {
            errorOut = "scene_load_failed";
            return false;
        }
        return true;
    }

    GameObject* obj = FindNamedObject(op.object);
    if (!obj) {
        errorOut = "unknown_object";
        return false;
    }

    if (op.kind == WorldOpKind::SetTransform) {
        if (op.hasPosition)
            obj->SetPosition({op.posX, op.posY, op.posZ});
        if (op.hasRotationEuler) {
            obj->SetRotation({op.rotX, op.rotY, op.rotZ});
        } else if (op.hasRotationQuat) {
            obj->SetRotation(EulerZyxDegreesFromQuaternion(op.quatX, op.quatY, op.quatZ, op.quatW));
        }
        if (op.hasScale)
            obj->SetScale({op.scaleX, op.scaleY, op.scaleZ});
        SyncPhysicsAfterTeleport(*obj, op.zeroVelocity);
        return true;
    }

    RigidbodyComponent* rc = nullptr;
    if (!IsDynamicBody(*obj, &rc) || !rc) {
        errorOut = "not_dynamic";
        return false;
    }
    Rigidbody& body = rc->Body();

    if (op.kind == WorldOpKind::SetVelocity) {
        if (op.hasLinear)
            body.SetVelocity({op.linX, op.linY, op.linZ});
        if (op.hasAngular)
            body.SetAngularVelocity({op.angX, op.angY, op.angZ});
        return true;
    }

    if (op.kind == WorldOpKind::ApplyImpulse) {
        const DirectX::XMFLOAT3 impulse{op.impX, op.impY, op.impZ};
        if (op.hasPoint) {
            body.AddForceAtPosition(impulse, {op.pointX, op.pointY, op.pointZ},
                obj->GetPosition(), obj->GetRotation(), ForceMode::Impulse);
        } else {
            body.AddForce(impulse, ForceMode::Impulse);
        }
        return true;
    }

    if (op.kind == WorldOpKind::ApplyTorqueImpulse) {
        // Keep inv-inertia frame current for paused teleports.
        DirectX::XMFLOAT3 pos = obj->GetPosition();
        DirectX::XMFLOAT3 rot = obj->GetRotation();
        body.Integrate(pos, rot, 0.0);
        body.AddTorque({op.torX, op.torY, op.torZ}, ForceMode::Impulse);
        return true;
    }

    errorOut = "unknown_world_op";
    return false;
}
#endif
