/*****************************************************
 * File: component_factory.h
 * Project: d3d11_engine
 * Author: Terry
 * Created: 2026-03-12
 * Description: component_factory.h – Singleton registry that maps component
 *              type-name strings to creator functions, enabling JSON-driven
 *              component instantiation without hard-coded switch/case logic.
 *****************************************************/
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include "component.h"

// Singleton factory: maps GetTypeName() strings to creator lambdas.
//
// Register new component types at startup:
//   ComponentFactory::Instance().Register("EnemyManager",
//       []{ return std::make_unique<EnemyManagerComponent>(); });
//
// Then create by name:
//   auto comp = ComponentFactory::Instance().Create("EnemyManager");
class ComponentFactory {
public:
    using CreatorFunc = std::function<std::unique_ptr<Component>()>;

    static ComponentFactory& Instance();

    // Register a creator for the given type name.
    // Re-registering the same name overwrites the previous creator.
    void Register(const std::string& typeName, CreatorFunc creator);

    // Create a component by its registered type name.
    // Returns nullptr if the name is not registered.
    std::unique_ptr<Component> Create(const std::string& typeName) const;

    // Returns true if a creator is registered for typeName.
    bool IsRegistered(const std::string& typeName) const;

private:
    ComponentFactory() = default;
    std::unordered_map<std::string, CreatorFunc> m_creators;
};

// Convenience macro — call once per type (e.g. in a registration function).
// Requires the type's default constructor and that the header is included.
#define REGISTER_COMPONENT(TypeName)                                      \
    ComponentFactory::Instance().Register(                                \
        TypeName{}.GetTypeName(),                                         \
        []() -> std::unique_ptr<Component> {                              \
            return std::make_unique<TypeName>();                          \
        })
