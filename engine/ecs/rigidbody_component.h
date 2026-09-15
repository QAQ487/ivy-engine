/*****************************************************
 * File: engine/ecs/rigidbody_component.h
 * Project: d3d11_engine
 * Author: Terry Yeung
 * -----------------------------------------------------------------------------
 * Serialisable Rigidbody settings for physics bodies.
 * Registered with ComponentFactory as "Rigidbody".
 *
 * Prefab JSON example:
 *   "Rigidbody": {
 *     "useGravity": true,
 *     "isKinematic": false,
 *     "mass": 1.0,
 *     "drag": 0.05,
 *     "restitution": 0.0,
 *     "friction": 0.0
 *   }
 *****************************************************/
#pragma once
#include "component.h"
#include "engine/physics/core/rigidbody.h"
#include <string>

class RigidbodyComponent : public Component {
public:
    const char* GetTypeName() const override { return "Rigidbody"; }

    Rigidbody& Body() { return m_body; }
    const Rigidbody& Body() const { return m_body; }

    void ApplyToOwner();
    void OnReset() override;

    std::string Serialize() const override;
    void Deserialize(const std::string& jsonStr) override;

private:
    Rigidbody m_body;
};
