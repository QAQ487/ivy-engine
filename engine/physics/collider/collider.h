/**===========================================================================

 File   :  [collider.h]

 Author : Hayato Otsuki
 Date   : 2025/10/29
 -----------------------------------------------------------------------------

 ===========================================================================**/

#ifndef COLLIDER_H
#define COLLIDER_H

#include <DirectXMath.h>
#include <vector>

 // Forward-declare the classes and enums we need
class GameObject;
class Model3D;
enum class Tag;

enum class ColliderType {
    NONE,
    BOX,
    SPHERE,
    CAPSULE,
    MESH,
    CYLINDER,
};

struct ContactInfo {
    float signedDist = FLT_MAX;
    DirectX::XMFLOAT3 normalLocal = {0, 0, 0};  // Local normal relative to box
    bool isInside = false;
};

//-----------------------------------------------
// Collider base class
//-----------------------------------------------
class Collider {
protected:
    ColliderType m_type = ColliderType::NONE;
    GameObject* m_owner = nullptr; // Reference to owning GameObject
    DirectX::XMFLOAT3 m_center = {0, 0, 0}; // Local offset from GameObject
    DirectX::XMFLOAT3 m_size = {1, 1, 1}; // Default size
    bool m_isTrigger = false; // If true, collision detection occurs but no physical resolution

public:
    Collider(ColliderType type, GameObject* owner) : m_type(type), m_owner(owner) {}
    virtual ~Collider() {}

    ColliderType GetType() const { return m_type; }
    void SetOwner(GameObject* owner) { m_owner = owner; }
    GameObject* GetOwner() const { return m_owner; }

    void SetCenter(const DirectX::XMFLOAT3& center) { m_center = center; }  // Set collider's local center
    DirectX::XMFLOAT3 GetCenter() const { return m_center; }

    void SetIsTrigger(bool isTrigger) { m_isTrigger = isTrigger; }
    bool IsTrigger() const { return m_isTrigger; }

    // World space center (considers owner's scale, rotation, position)
    DirectX::XMFLOAT3 GetWorldCenter() const;

    // Collision interface
    virtual bool CheckCollision(const Collider* other) const = 0;
    virtual void ResolveCollision(Collider* other) = 0;

    // Debug drawing (default green wireframe)
    virtual void DebugDraw(const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f }) = 0;
};

//-----------------------------------------------
// Box Collider
//-----------------------------------------------
class BoxCollider : public Collider {
public:
    BoxCollider(GameObject* owner) : Collider(ColliderType::BOX, owner) {}
    void SetSize(const DirectX::XMFLOAT3& size) { m_size = size; }  // Box collider full size (world space, not scaled by owner)
    // Returns the full world-space size of the box (width, height, depth).
    // This is NOT multiplied by the owner's scale; callers must pass absolute world-space dimensions.
    DirectX::XMFLOAT3 GetSize() const;

    bool CheckCollision(const Collider* other) const override;
    void ResolveCollision(Collider* other) override;

    void DebugDraw(const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f }) override;
};

//-----------------------------------------------
// Sphere Collider
//-----------------------------------------------
class SphereCollider : public Collider {
private:
    float m_radius = 0.5f;

public:
    SphereCollider(GameObject* owner) : Collider(ColliderType::SPHERE, owner) {}
    void SetRadius(float r) { m_radius = r; }
    float GetRadius() const { return m_radius; }

    bool CheckCollision(const Collider* other) const override;
    void ResolveCollision(Collider* other) override;

    void DebugDraw(const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f }) override;
};

//-----------------------------------------------
// Capsule Collider
//-----------------------------------------------
class CapsuleCollider : public Collider {
private:
    float m_radius = 0.5f;
    float m_height = 2.0f;

public:
    CapsuleCollider(GameObject* owner) : Collider(ColliderType::CAPSULE, owner) {}
    void SetRadius(float r) { m_radius = r; }
    void SetHeight(float h) { m_height = h; }

    float GetRadius() const { return m_radius; }
    float GetHeight() const { return m_height; }

    bool CheckCollision(const Collider* other) const override;
    void ResolveCollision(Collider* other) override;

    void DebugDraw(const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f }) override;
};

//-----------------------------------------------
// Cylinder Collider - vertical Y-axis disc/slab (radius + height, world units)
//-----------------------------------------------
class CylinderCollider : public Collider {
private:
    float m_radius = 0.5f;
    float m_height = 1.0f;

public:
    CylinderCollider(GameObject* owner) : Collider(ColliderType::CYLINDER, owner) {}
    void SetRadius(float r) { m_radius = r; }
    void SetHeight(float h) { m_height = h; }
    float GetRadius() const { return m_radius; }
    float GetHeight() const { return m_height; }

    bool CheckCollision(const Collider* other) const override;
    void ResolveCollision(Collider* other) override;
    void DebugDraw(const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f }) override;
};

//-----------------------------------------------
// Mesh Collider - Triangle mesh collision support
// Note: Accounts for GameObject's m_scale during collision detection
//-----------------------------------------------
class MeshCollider : public Collider {
private:
    std::vector<DirectX::XMFLOAT3> m_vertices;  // Mesh vertex positions (local coordinates)
    std::vector<unsigned int> m_indices;         // Triangle indices (3 vertices per triangle)
    float m_boundingSphereRadius = 0.0f;         // Local-space radius (owner scale applied at query time)
    DirectX::XMFLOAT3 m_boundingSphereCenter = {0, 0, 0}; // Local space center
    float m_sizePadding = 0.0f;                  // Extra padding for collision size

public:
    MeshCollider(GameObject* owner) : Collider(ColliderType::MESH, owner) {}

    // Set mesh data from vertices and indices
    void SetMeshData(const std::vector<DirectX::XMFLOAT3>& vertices, 
                     const std::vector<unsigned int>& indices);
    
    // Build mesh data from Model3D
    void BuildFromModel(const Model3D& model);
    
    // Set/Get size padding - adds extra size to the collision mesh
    void SetSizePadding(float padding) { m_sizePadding = padding; }
    float GetSizePadding() const { return m_sizePadding; }

    const std::vector<DirectX::XMFLOAT3>& GetVertices() const { return m_vertices; }
    const std::vector<unsigned int>& GetIndices() const { return m_indices; }
    float GetBoundingSphereRadius() const { return m_boundingSphereRadius; }
    DirectX::XMFLOAT3 GetBoundingSphereCenter() const { return m_boundingSphereCenter; }

    // Local-space radius scaled by the owner's current transform (for broad-phase queries).
    float GetWorldBoundingSphereRadius() const;

    // World-space axis-aligned bounds (full size, same convention as BoxCollider::GetSize).
    void GetWorldAABB(DirectX::XMFLOAT3& outCenter, DirectX::XMFLOAT3& outSize) const;

    // Raycast against mesh (returns true if hit, outputs hit point and normal)
    // Note: Considers owner's scale, rotation, and position in world space
    bool Raycast(const DirectX::XMFLOAT3& rayOrigin,
                 const DirectX::XMFLOAT3& rayDirection,
                 float maxDistance,
                 DirectX::XMFLOAT3& outHitPoint,
                 DirectX::XMFLOAT3& outHitNormal) const;

    bool CheckCollision(const Collider* other) const override;
    void ResolveCollision(Collider* other) override;
    void DebugDraw(const DirectX::XMFLOAT4& color = { 0.0f, 1.0f, 0.0f, 1.0f }) override;

private:
    // Calculate bounding sphere from vertices (considers owner's scale)
    void CalculateBoundingSphere();
    
    // Triangle collision helpers
    bool RayIntersectsTriangle(const DirectX::XMFLOAT3& rayOrigin, 
                               const DirectX::XMFLOAT3& rayDir,
                               const DirectX::XMFLOAT3& v0,
                               const DirectX::XMFLOAT3& v1,
                               const DirectX::XMFLOAT3& v2,
                               float& t) const;
    
    // Triangle-sphere intersection test
    bool TriangleIntersectsSphere(const DirectX::XMFLOAT3& v0,
                                  const DirectX::XMFLOAT3& v1,
                                  const DirectX::XMFLOAT3& v2,
                                  const DirectX::XMFLOAT3& sphereCenter,
                                  float sphereRadius,
                                  DirectX::XMFLOAT3& outNormal,
                                  float& outPenetration) const;
};

#endif