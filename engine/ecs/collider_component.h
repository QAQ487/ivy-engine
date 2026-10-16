#pragma once
#include "component.h"
#include "engine/physics/collider/collider.h"
#include <DirectXMath.h>
#include <string>

// CapsuleCollider height is the segment between sphere centers, not the
// total vertical extent. Radius is the farthest mesh vertex from the Y
// axis. Both caps already occupy `radius` at each end, so
// segment = totalHeight - 2*radius.
//
// When the mesh is shorter than it is wide (halfHeight < radius), the
// segment would be negative. It clamps to 0 and the collider becomes a
// sphere of `radius`. That sphere covers the horizontal vertices and
// therefore extends past the mesh along Y by (radius - halfHeight). The
// overhang is unavoidable: a capsule that covers those wide vertices
// cannot also stay inside the shorter height.
//
// Keeping the poles flush with the mesh also cannot cover off-axis
// vertices on the top or bottom face (box corners, cylinder rims). A
// hemisphere only has its full radius at the sphere center, so those
// corners stay outside unless the caps extend past the mesh.
inline void ComputeCapsuleFit(float halfHeight, float cylinderRadius,
    float& outRadius, float& outSegmentHeight) {
    outRadius = cylinderRadius;
    const float segment = 2.0f * halfHeight - 2.0f * cylinderRadius;
    outSegmentHeight = segment > 0.0f ? segment : 0.0f;
}

class ColliderComponent : public Component {
public:
    ColliderType colliderType   = ColliderType::NONE;
    DirectX::XMFLOAT3 colliderCenter = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 halfExtents    = { 0.5f, 0.5f, 0.5f };
    float radius         = 0.5f;
    float height         = 2.0f;
    float meshSizePadding = 0.0f;
    bool showDebugCollider = true;
    bool isTrigger = false;
    std::string physicsMaterial;

    const char* GetTypeName() const override { return "Collider"; }

    void ApplyToOwner();
    void ClearFromOwner();

    std::string Serialize() const override;
    void Deserialize(const std::string& jsonStr) override;
};
