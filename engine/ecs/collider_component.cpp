#include "collider_component.h"
#include "engine/ecs/game_object.h"
#include "engine/physics/physics_material.h"
#include "model_asset_cache.h"
#include "rigidbody_component.h"
#include "visual_component.h"

#include <Windows.h>


void ColliderComponent::ClearFromOwner() {
    if (!owner) return;
    // Host-owned colliders were allocated by the EXE on behalf of a script DLL;
    // deleting them here (potentially from a different module's heap) is unsafe.
    // Mirror GameObject::~GameObject()'s ownership check instead of unconditionally deleting.
    if (owner->m_collider && !owner->m_colliderOwnedByHost)
        delete owner->m_collider;
    owner->m_collider = nullptr;
    owner->m_colliderOwnedByHost = false;
}

void ColliderComponent::ApplyToOwner() {
    if (!owner) return;

    ClearFromOwner();
    owner->m_debugCollider = showDebugCollider;

    if (colliderType == ColliderType::NONE) return;

    Model3D* model = nullptr;
    if (auto* vc = owner->GetComponent<VisualComponent>())
        model = ModelAssetCache::Instance().GetModel(vc->modelPath);

    switch (colliderType) {
    case ColliderType::BOX:
        owner->CreateBoxCollider(
            { halfExtents.x * 2.0f, halfExtents.y * 2.0f, halfExtents.z * 2.0f },
            colliderCenter);
        break;
    case ColliderType::SPHERE:
        owner->CreateSphereCollider(radius, colliderCenter);
        break;
    case ColliderType::CAPSULE:
        owner->CreateCapsuleCollider(radius, height, colliderCenter);
        break;
    case ColliderType::CYLINDER:
        owner->CreateCylinderCollider(radius, height, colliderCenter);
        break;
    case ColliderType::MESH:
        if (model)
            owner->CreateMeshCollider(*model, colliderCenter, meshSizePadding);
        else
            OutputDebugStringA("[ColliderComponent] Mesh collider skipped: no model.\n");
        break;
    default:
        break;
    }

    if (owner->m_collider) {
        owner->m_collider->SetIsTrigger(isTrigger);
        ContactSurface surface;
        if (!physicsMaterial.empty() &&
            !PhysicsMaterial_LoadFromFile(physicsMaterial, surface)) {
            OutputDebugStringA(
                "[ColliderComponent] Physics material load failed; using rigidbody contact fallback.\n");
        }
        owner->m_collider->SetContactSurface(surface);
    }

    // Static colliders default to kinematic. Authored RigidbodyComponent keeps
    // its own isKinematic / mass / drag from the scene (do not overwrite).
    const bool hadRigidbody = owner->HasComponent<RigidbodyComponent>();
    if (!hadRigidbody)
        owner->GetRigidbody().SetIsKinematic(true);
}
