#include "engine/physics/physics_material.h"

#include "libraries/nlohmann/json.hpp"

#include <algorithm>
#include <fstream>

using json = nlohmann::json;

namespace {
float ClampNonNegative(float value) {
    return value < 0.0f ? 0.0f : value;
}

constexpr float kJNormalEpsilon = 1e-6f;
// Fallback when ResolveContact cannot estimate mg|n·up|Δt (jn already 0 and
// no synthetic normal was provided). ~g/60 for unit mass.
constexpr float kRestingNormalImpulse = 0.16f;
}

std::string PhysicsMaterial_ReadPathFromColliderJson(const std::string& jsonStr) {
    try {
        const json j = json::parse(jsonStr);
        return j.value("physicsMaterial", "");
    } catch (...) {
        return {};
    }
}

bool PhysicsMaterial_LoadFromFile(const std::string& path, ContactSurface& out) {
    out = ContactSurface{};
    if (path.empty())
        return false;

    std::ifstream in(path);
    if (!in.is_open())
        return false;

    json root;
    try {
        in >> root;
    } catch (...) {
        return false;
    }

    if (!root.is_object())
        return false;

    const float staticFriction = ClampNonNegative(root.value("staticFriction", 0.0f));
    float dynamicFriction = ClampNonNegative(root.value("dynamicFriction", 0.0f));
    if (dynamicFriction > staticFriction)
        dynamicFriction = staticFriction;
    const float restitution = ClampNonNegative(root.value("restitution", 0.0f));

    out.assigned = true;
    out.staticFriction = staticFriction;
    out.dynamicFriction = dynamicFriction;
    out.restitution = restitution;
    return true;
}

ContactSurface PhysicsMaterial_FromRigidbody(float friction, float restitution) {
    ContactSurface surface;
    const float mu = ClampNonNegative(friction);
    surface.assigned = false;
    surface.staticFriction = mu;
    surface.dynamicFriction = mu;
    surface.restitution = ClampNonNegative(restitution);
    return surface;
}

CombinedContactSurface PhysicsMaterial_Combine(const ContactSurface& a, const ContactSurface& b) {
    CombinedContactSurface combined;
    combined.muStatic = (std::min)(a.staticFriction, b.staticFriction);
    combined.muDynamic = (std::min)(a.dynamicFriction, b.dynamicFriction);
    if (combined.muDynamic > combined.muStatic)
        combined.muDynamic = combined.muStatic;
    combined.restitution = (std::max)(a.restitution, b.restitution);
    return combined;
}

ContactFrictionDecision PhysicsMaterial_DecideFriction(
    float vtLen,
    float invSum,
    float jNormalAbs,
    float muStatic,
    float muDynamic)
{
    ContactFrictionDecision decision;
    if (invSum <= 0.0f)
        return decision;

    const float vt = vtLen < 0.0f ? 0.0f : vtLen;
    const float muS = ClampNonNegative(muStatic);
    float muD = ClampNonNegative(muDynamic);
    if (muD > muS)
        muD = muS;

    if (jNormalAbs <= kJNormalEpsilon) {
        // Resting contact with no bounce impulse this frame. Do NOT require
        // vt < kStickSpeed: on a slope gravity alone makes vt ≈ g sinθ Δt
        // (~0.09 at 32°/60Hz), which is already above that threshold, so a
        // high-μ rubber pad would never enter the static cone and would slide.
        const float jStick = vt / invSum;
        const float jMaxStatic = muS * kRestingNormalImpulse;
        if (jStick <= jMaxStatic) {
            decision.mode = ContactFrictionMode::ResidualStick;
            decision.jFriction = jStick;
            return decision;
        }
        if (muD > 0.0f && vt > 1e-6f) {
            decision.mode = ContactFrictionMode::DynamicClamp;
            decision.jFriction = (std::min)(jStick, muD * kRestingNormalImpulse);
        }
        return decision;
    }

    const float jStick = vt / invSum;
    const float jMaxStatic = muS * jNormalAbs;
    if (jStick <= jMaxStatic) {
        decision.mode = ContactFrictionMode::StaticStick;
        decision.jFriction = jStick;
        return decision;
    }

    decision.mode = ContactFrictionMode::DynamicClamp;
    decision.jFriction = (std::min)(jStick, muD * jNormalAbs);
    return decision;
}
