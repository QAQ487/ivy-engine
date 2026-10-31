// collider_component_access.h
// ECS: collider component access declarations.

#pragma once
#include "collider_component.h"
#include "game_object.h"
inline bool IsColliderComponentType(const std::string& name) {
    return name=="Collider"||name=="BoxCollider"||name=="SphereCollider"||name=="CapsuleCollider"
        ||name=="CylinderCollider"||name=="MeshCollider";
}
inline ColliderComponent* FindColliderComponent(GameObject* object) {
    if(!object)return nullptr;
    for(const auto& entry:object->GetComponents()) {
        auto* component=entry.second.get();
        if(component&&IsColliderComponentType(component->GetTypeName()))return static_cast<ColliderComponent*>(component);
    }
    return nullptr;
}
inline void RemoveColliderComponent(GameObject* object) {
    if(!object)return;
    if(auto* collider=FindColliderComponent(object))collider->ClearFromOwner();
    object->RemoveComponent<ColliderComponent>();
    object->RemoveComponent<BoxColliderComponent>();
    object->RemoveComponent<SphereColliderComponent>();
    object->RemoveComponent<CapsuleColliderComponent>();
    object->RemoveComponent<CylinderColliderComponent>();
    object->RemoveComponent<MeshColliderComponent>();
}

inline ColliderComponent* FindColliderComponent(const std::unique_ptr<GameObject>& object) {
    return FindColliderComponent(object.get());
}
