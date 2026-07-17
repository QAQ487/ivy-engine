#pragma once
#include "component.h"
#include "engine/physics/collider/collider.h"
#include <DirectXMath.h>
#include <string>

class ColliderComponent : public Component {
public:
    ColliderType colliderType   = ColliderType::NONE;
    DirectX::XMFLOAT3 colliderCenter = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 halfExtents    = { 0.5f, 0.5f, 0.5f };
    float radius         = 0.5f;
    float height         = 2.0f;
    float meshSizePadding = 0.0f;
    bool showDebugCollider = true;

    const char* GetTypeName() const override { return "Collider"; }

    void ApplyToOwner();
    void ClearFromOwner();

    std::string Serialize() const override;
    void Deserialize(const std::string& jsonStr) override;
};
