// collider_component_serialization.cpp
// ECS: collider component serialization implementation.

#include "collider_component.h"
#include "libraries/nlohmann/json.hpp"
#include <Windows.h>
#include <algorithm>
#include <cmath>
using json=nlohmann::json;

std::string ColliderComponent::Serialize() const {
    json j;
    j["colliderType"] = static_cast<int>(colliderType);
    j["colliderCenter"] = { {"x", colliderCenter.x}, {"y", colliderCenter.y}, {"z", colliderCenter.z} };
    j["halfExtents"] = { {"x", halfExtents.x}, {"y", halfExtents.y}, {"z", halfExtents.z} };
    j["radius"] = radius;
    j["height"] = height;
    j["meshSizePadding"] = meshSizePadding;
    j["showDebugCollider"] = showDebugCollider;
    j["isTrigger"] = isTrigger;
    j["physicsMaterial"] = physicsMaterial;
    return j.dump();
}

std::string ColliderComponent::SerializeShape(ColliderType shape) const {
    auto data=json::parse(ColliderComponent::Serialize());
    data.erase("colliderType");
    if(shape!=ColliderType::BOX)data.erase("halfExtents");
    if(shape!=ColliderType::SPHERE&&shape!=ColliderType::CAPSULE&&shape!=ColliderType::CYLINDER)data.erase("radius");
    if(shape!=ColliderType::CAPSULE&&shape!=ColliderType::CYLINDER)data.erase("height");
    if(shape!=ColliderType::MESH)data.erase("meshSizePadding");
    return data.dump();
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
        isTrigger = j.value("isTrigger", false);
        physicsMaterial = j.value("physicsMaterial", "");
    } catch (...) {
        OutputDebugStringA("[ColliderComponent] Deserialize failed.\n");
    }
    const int type=static_cast<int>(colliderType);
    if(type<0||type>static_cast<int>(ColliderType::CYLINDER))colliderType=ColliderType::NONE;
    auto finite=[](float value,float fallback,float minimum) {
        return std::isfinite(value)?(std::clamp)(value,minimum,100000.f):fallback;
    };
    colliderCenter.x=finite(colliderCenter.x,0,-100000);
    colliderCenter.y=finite(colliderCenter.y,0,-100000);
    colliderCenter.z=finite(colliderCenter.z,0,-100000);
    halfExtents.x=finite(halfExtents.x,.5f,.001f);
    halfExtents.y=finite(halfExtents.y,.5f,.001f);
    halfExtents.z=finite(halfExtents.z,.5f,.001f);
    radius=finite(radius,.5f,.001f);height=finite(height,2,0);meshSizePadding=finite(meshSizePadding,0,0);
}
