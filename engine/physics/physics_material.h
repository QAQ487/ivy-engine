// physics_material.h
// Declares ContactSurface.

#pragma once

#include <string>

// Contact-surface coefficients used by ResolveContact. assigned is true only
// when a Physics Material asset loaded onto the collider; otherwise the
// solver falls back to Rigidbody friction and restitution.
struct ContactSurface {
    bool assigned = false;
    float staticFriction = 0.0f;
    float dynamicFriction = 0.0f;
    float restitution = 0.0f;
};

struct CombinedContactSurface {
    float muStatic = 0.0f;
    float muDynamic = 0.0f;
    float restitution = 0.0f;
};

enum class ContactFrictionMode {
    Skip,
    ResidualStick,
    StaticStick,
    DynamicClamp
};

struct ContactFrictionDecision {
    ContactFrictionMode mode = ContactFrictionMode::Skip;
    float jFriction = 0.0f;
};

std::string PhysicsMaterial_ReadPathFromColliderJson(const std::string& jsonStr);

bool PhysicsMaterial_LoadFromFile(const std::string& path, ContactSurface& out);

ContactSurface PhysicsMaterial_FromRigidbody(float friction, float restitution);

CombinedContactSurface PhysicsMaterial_Combine(const ContactSurface& a, const ContactSurface& b);

// Coulomb friction decision. When jNormalAbs is ~0 (resting contact), callers
// should pass a synthetic mg|n·up|Δt so slopes can stick (μs >= tanθ).
ContactFrictionDecision PhysicsMaterial_DecideFriction(
    float vtLen,
    float invSum,
    float jNormalAbs,
    float muStatic,
    float muDynamic);
