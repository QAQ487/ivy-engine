/*****************************************************
 * File: transform.cpp
 * Project: d3d11_engine
 * Author: Terry Yeung
 * -----------------------------------------------------------------------------
 * REFACTOR (2026-03-13)
 *   Includes game_object.h directly (scene_node.h is now just an alias).
 *   No logic changes.
 *****************************************************/
#include "transform.h"
#include "engine/ecs/game_object.h"

#include <Windows.h>
#include "libraries/nlohmann/json.hpp"

#include <cmath>
#include "engine/debug_draw/debug_ofstream.h"

using namespace DirectX;
using json = nlohmann::json;

namespace engine {

namespace {

XMFLOAT3 EulerZyxDegreesFromRotationMatrix(XMMATRIX rot) {
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
        XMConvertToDegrees(rotX),
        XMConvertToDegrees(rotY),
        XMConvertToDegrees(rotZ)
    };
}

} // namespace

void Transform::OnAttach() {
    SnapshotStartState();
}

void Transform::OnReset() {
    localPosition = m_startPosition;
    localRotation = m_startRotation;
    localScale    = m_startScale;
}

void Transform::SnapshotStartState() {
    m_startPosition = localPosition;
    m_startRotation = localRotation;
    m_startScale    = localScale;
}

XMMATRIX Transform::GetLocalMatrix() const {
    XMMATRIX S  = XMMatrixScaling(localScale.x, localScale.y, localScale.z);
    XMMATRIX Rx = XMMatrixRotationX(XMConvertToRadians(localRotation.x));
    XMMATRIX Ry = XMMatrixRotationY(XMConvertToRadians(localRotation.y));
    XMMATRIX Rz = XMMatrixRotationZ(XMConvertToRadians(localRotation.z));
    XMMATRIX R  = Rz * Ry * Rx; // ZYX Euler convention
    XMMATRIX T  = XMMatrixTranslation(localPosition.x, localPosition.y, localPosition.z);
    return S * R * T;
}

XMMATRIX Transform::GetWorldMatrix() const {
    XMMATRIX local = GetLocalMatrix();
    if (!owner) return local;

    const GameObject* parent = owner->GetParent();
    if (!parent) return local;

    const Transform* parentXform = parent->GetComponent<Transform>();
    if (!parentXform) return local;

    return local * parentXform->GetWorldMatrix();
}

XMFLOAT3 Transform::GetWorldPosition() const {
    XMFLOAT3 pos;
    XMStoreFloat3(&pos, GetWorldMatrix().r[3]);
    return pos;
}

void Transform::GetWorldPositionRotationScale(XMFLOAT3& outPosition,
                                               XMFLOAT3& outRotation,
                                               XMFLOAT3& outScale) const {
    const GameObject* parent = owner ? owner->GetParent() : nullptr;
    if (!parent || !parent->GetComponent<Transform>()) {
        // No parent transform in the chain, so world pose equals local pose;
        // skip the decompose.
        outPosition = localPosition;
        outRotation = localRotation;
        outScale    = localScale;
        return;
    }

    XMVECTOR scale, rotQuat, translation;
    XMMatrixDecompose(&scale, &rotQuat, &translation, GetWorldMatrix());

    XMStoreFloat3(&outPosition, translation);
    XMStoreFloat3(&outScale, scale);
    outRotation = EulerZyxDegreesFromRotationMatrix(XMMatrixRotationQuaternion(rotQuat));
}

void Transform::SetFromLocalMatrix(XMMATRIX m) {
    XMVECTOR scale, rotQuat, translation;
    XMMatrixDecompose(&scale, &rotQuat, &translation, m);

    XMStoreFloat3(&localPosition, translation);
    XMStoreFloat3(&localScale, scale);

    const XMMATRIX rotMat = XMMatrixRotationQuaternion(rotQuat);
    localRotation = EulerZyxDegreesFromRotationMatrix(rotMat);
}

std::string Transform::Serialize() const {
    json j;
    j["posX"]   = localPosition.x;
    j["posY"]   = localPosition.y;
    j["posZ"]   = localPosition.z;
    j["rotX"]   = localRotation.x;
    j["rotY"]   = localRotation.y;
    j["rotZ"]   = localRotation.z;
    j["scaleX"] = localScale.x;
    j["scaleY"] = localScale.y;
    j["scaleZ"] = localScale.z;
    return j.dump();
}

void Transform::Deserialize(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        localPosition.x = j.value("posX",   0.0f);
        localPosition.y = j.value("posY",   0.0f);
        localPosition.z = j.value("posZ",   0.0f);
        localRotation.x = j.value("rotX",   0.0f);
        localRotation.y = j.value("rotY",   0.0f);
        localRotation.z = j.value("rotZ",   0.0f);
        localScale.x    = j.value("scaleX", 1.0f);
        localScale.y    = j.value("scaleY", 1.0f);
        localScale.z    = j.value("scaleZ", 1.0f);
        SnapshotStartState();
    } catch (...) {
        hal::DebugWriteA("[engine::Transform] Deserialize: JSON parse error.\n");
    }
}

} // namespace engine
