/*****************************************************
 * File: engine/ecs/rigidbody_component.cpp
 * Project: d3d11_engine
 * Author: Terry
 *****************************************************/
#include "rigidbody_component.h"

#include "libraries/nlohmann/json.hpp"

#include <Windows.h>

using json = nlohmann::json;

void RigidbodyComponent::ApplyToOwner() {
    (void)owner;
}

void RigidbodyComponent::OnReset() {
    m_body.ClearAllVelocity();
    m_body.ClearAllForces();
}

std::string RigidbodyComponent::Serialize() const {
    json j;
    j["useGravity"] = m_body.UseGravity();
    j["isKinematic"] = m_body.IsKinematic();
    j["mass"] = m_body.GetMass();
    j["drag"] = m_body.GetDrag();
    j["restitution"] = m_body.GetRestitution();
    return j.dump();
}

void RigidbodyComponent::Deserialize(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        m_body.SetUseGravity(j.value("useGravity", false));
        m_body.SetIsKinematic(j.value("isKinematic", false));
        m_body.SetMass(j.value("mass", 1.0f));
        m_body.SetDrag(j.value("drag", 0.05f));
        m_body.SetRestitution(j.value("restitution", 0.0f));
        m_body.ClearAllVelocity();
        m_body.ClearAllForces();
    } catch (...) {
        OutputDebugStringA("[RigidbodyComponent] Deserialize failed.\n");
    }
}
