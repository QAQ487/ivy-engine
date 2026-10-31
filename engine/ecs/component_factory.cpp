/*****************************************************
 * File: component_factory.cpp
 * Project: d3d11_engine
 * Author: Terry Yeung
 * Created: 2026-03-12
 * Description: component_factory.cpp  - ComponentFactory singleton
 *              implementation.
 *****************************************************/
#include "component_factory.h"

ComponentFactory& ComponentFactory::Instance() {
    static ComponentFactory s_instance;
    return s_instance;
}

void ComponentFactory::Register(const std::string& typeName, CreatorFunc creator) {
    m_creators[typeName] = std::move(creator);
}

std::unique_ptr<Component> ComponentFactory::Create(const std::string& typeName) const {
    auto it = m_creators.find(typeName);
    if (it == m_creators.end()) return nullptr;
    return it->second();
}

bool ComponentFactory::IsRegistered(const std::string& typeName) const {
    return m_creators.count(typeName) > 0;
}
