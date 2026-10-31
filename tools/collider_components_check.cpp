// collider_components_check.cpp
// Validates ECS collider component wiring.

#include "engine/ecs/collider_component.h"
#include "engine/ecs/component_factory.h"
#include "libraries/nlohmann/json.hpp"
#include <cstdlib>
#include <iostream>
#include <cstring>

static void Check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

template<class T>
static void CheckShape(const char* name, ColliderType shape) {
    T source;
    Check(std::strcmp(source.GetTypeName(), name) == 0, "shape type identity");
    Check(source.colliderType == shape, "constructor shape");
    source.colliderCenter = { 1.25f, -2.5f, 3.75f };
    source.halfExtents = { 4.0f, 5.0f, 6.0f };
    source.radius = 2.25f;
    source.height = 7.5f;
    source.meshSizePadding = 0.125f;
    source.showDebugCollider = false;
    source.isTrigger = true;
    source.physicsMaterial = "rom/physics/test.json";
    const auto expected = nlohmann::json::parse(source.Serialize());
    Check(!expected.contains("colliderType"), "shape-specific JSON omits shape discriminator");
    Check(expected.contains("halfExtents") == (shape == ColliderType::BOX), "box-only fields");
    Check(expected.contains("radius") == (shape == ColliderType::SPHERE || shape == ColliderType::CAPSULE || shape == ColliderType::CYLINDER), "round shape fields");
    Check(expected.contains("height") == (shape == ColliderType::CAPSULE || shape == ColliderType::CYLINDER), "height fields");
    Check(expected.contains("meshSizePadding") == (shape == ColliderType::MESH), "mesh-only fields");
    T restored;
    restored.Deserialize(source.Serialize());
    Check(nlohmann::json::parse(restored.Serialize()) == expected, "all parameters roundtrip");

    auto mismatched = expected;
    mismatched["colliderType"] = static_cast<int>(ColliderType::NONE);
    restored.Deserialize(mismatched.dump());
    Check(restored.colliderType == shape, "authored type overrides mismatched serialized shape");
    Check(nlohmann::json::parse(restored.Serialize()) == expected, "mismatched shape preserves parameters");
    restored.Deserialize("{}");
    Check(restored.colliderType == shape, "missing serialized shape preserves authored type");

    ColliderComponent legacy;
    auto legacyData = expected;
    legacyData["colliderType"] = static_cast<int>(shape);
    legacy.Deserialize(legacyData.dump());
    Check(legacy.colliderType == shape, "legacy shape loads");
    Check(std::strcmp(legacy.GetTypeName(), name) == 0, "legacy shape maps to new identity");
    Check(nlohmann::json::parse(legacy.SerializeShape(shape)) == expected, "legacy parameters preserved");

    auto& factory = ComponentFactory::Instance();
    factory.Register(name, [] { return std::make_unique<T>(); });
    Check(factory.IsRegistered(name), "factory registration");
    auto created = factory.Create(name);
    Check(created && dynamic_cast<T*>(created.get()), "factory creates concrete shape type");
    created->Deserialize(mismatched.dump());
    Check(std::strcmp(created->GetTypeName(), name) == 0, "factory identity after deserialize");
    Check(nlohmann::json::parse(created->Serialize()) == expected, "factory shape and parameter roundtrip");
}

int main() {
    CheckShape<BoxColliderComponent>("BoxCollider", ColliderType::BOX);
    CheckShape<SphereColliderComponent>("SphereCollider", ColliderType::SPHERE);
    CheckShape<CapsuleColliderComponent>("CapsuleCollider", ColliderType::CAPSULE);
    CheckShape<CylinderColliderComponent>("CylinderCollider", ColliderType::CYLINDER);
    CheckShape<MeshColliderComponent>("MeshCollider", ColliderType::MESH);
    Check(!ComponentFactory::Instance().Create("UnknownCollider"), "unknown factory name rejected");
    ColliderComponent invalid;
    invalid.Deserialize(R"({"radius":-5,"height":-3,"meshSizePadding":-1,"halfExtents":{"x":-4,"y":1e20,"z":0},"colliderCenter":{"x":-1e20,"y":1e20,"z":0},"colliderType":999})");
    Check(invalid.radius > 0 && invalid.height == 0 && invalid.meshSizePadding == 0, "negative dimensions bounded");
    Check(invalid.halfExtents.x > 0 && invalid.halfExtents.y == 100000 && invalid.halfExtents.z > 0, "extents bounded");
    Check(invalid.colliderCenter.x == -100000 && invalid.colliderCenter.y == 100000, "center bounded");
    Check(invalid.colliderType == ColliderType::NONE, "invalid legacy discriminator rejected");
    std::cout << "Collider component checks passed (5 shapes, serialization, legacy identity, factory).\n";
}
