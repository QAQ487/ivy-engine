#include "collider_component.h"
#include "engine/ecs/game_object.h"
#include "model_asset_cache.h"
#include "visual_component.h"

#include "libraries/nlohmann/json.hpp"
#include <Windows.h>

using json = nlohmann::json;

void ColliderComponent::ClearFromOwner() {
    if (!owner) return;
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

    owner->GetRigidbody().SetIsKinematic(true);
}

std::string ColliderComponent::Serialize() const {
    json j;
    j["colliderType"] = static_cast<int>(colliderType);
    j["colliderCenter"] = { {"x", colliderCenter.x}, {"y", colliderCenter.y}, {"z", colliderCenter.z} };
    j["halfExtents"] = { {"x", halfExtents.x}, {"y", halfExtents.y}, {"z", halfExtents.z} };
    j["radius"] = radius;
    j["height"] = height;
    j["meshSizePadding"] = meshSizePadding;
    j["showDebugCollider"] = showDebugCollider;
    return j.dump();
}

void ColliderComponent::Deserialize(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        colliderType = static_cast<ColliderType>(j.value("colliderType", 0));
        if (j.contains("colliderCenter")) {
            colliderCenter.x = j["colliderCenter"].value("x", 0.0f);
            colliderCenter.y = j["colliderCenter"].value("y", 0.0f);
            colliderCenter.z = j["colliderCenter"].value("z", 0.0f);
        }
        if (j.contains("halfExtents")) {
            halfExtents.x = j["halfExtents"].value("x", 0.5f);
            halfExtents.y = j["halfExtents"].value("y", 0.5f);
            halfExtents.z = j["halfExtents"].value("z", 0.5f);
        }
        radius = j.value("radius", 0.5f);
        height = j.value("height", 2.0f);
        meshSizePadding = j.value("meshSizePadding", 0.0f);
        showDebugCollider = j.value("showDebugCollider", true);
    } catch (...) {
        OutputDebugStringA("[ColliderComponent] Deserialize failed.\n");
    }
}
