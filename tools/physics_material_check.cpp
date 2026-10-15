#include "engine/physics/physics_material.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
int g_failed = 0;

void Expect(bool ok, const char* name) {
    if (ok) {
        std::printf("PASS  %s\n", name);
        return;
    }
    std::printf("FAIL  %s\n", name);
    ++g_failed;
}

bool NearlyEqual(float a, float b) {
    return std::fabs(a - b) <= 1e-5f;
}
}

int main() {
    Expect(PhysicsMaterial_ReadPathFromColliderJson("{}").empty(),
        "missing physicsMaterial key stays empty");
    Expect(PhysicsMaterial_ReadPathFromColliderJson("{\"physicsMaterial\":\"\"}").empty(),
        "empty physicsMaterial path stays empty");
    Expect(PhysicsMaterial_ReadPathFromColliderJson(
        "{\"physicsMaterial\":\"rom/physmat/example_rubber.physmat\"}")
            == "rom/physmat/example_rubber.physmat",
        "physicsMaterial path round-trips");

    ContactSurface rubber;
    Expect(PhysicsMaterial_LoadFromFile("rom/physmat/example_rubber.physmat", rubber),
        "load example_rubber.physmat");
    Expect(rubber.assigned, "example rubber is assigned");
    Expect(NearlyEqual(rubber.staticFriction, 0.9f), "example rubber staticFriction");
    Expect(NearlyEqual(rubber.dynamicFriction, 0.7f), "example rubber dynamicFriction");
    Expect(NearlyEqual(rubber.restitution, 0.6f), "example rubber restitution");

    ContactSurface missing;
    missing.assigned = true;
    missing.staticFriction = 1.0f;
    Expect(!PhysicsMaterial_LoadFromFile("rom/physmat/does_not_exist.physmat", missing),
        "failed load returns false");
    Expect(!missing.assigned && NearlyEqual(missing.staticFriction, 0.0f),
        "failed load stays unassigned");

    ContactSurface emptyPath;
    emptyPath.assigned = true;
    Expect(!PhysicsMaterial_LoadFromFile("", emptyPath), "empty path does not load");
    Expect(!emptyPath.assigned, "empty path stays unassigned");

    const ContactSurface ice = PhysicsMaterial_FromRigidbody(0.0f, 0.9f);
    const CombinedContactSurface slippery = PhysicsMaterial_Combine(rubber, ice);
    Expect(NearlyEqual(slippery.muStatic, 0.0f), "min static with friction-0 partner");
    Expect(NearlyEqual(slippery.muDynamic, 0.0f), "min dynamic with friction-0 partner");
    Expect(NearlyEqual(slippery.restitution, 0.9f), "max restitution");

    ContactSurface high;
    high.staticFriction = 0.8f;
    high.dynamicFriction = 0.6f;
    high.restitution = 0.2f;
    ContactSurface low;
    low.staticFriction = 0.3f;
    low.dynamicFriction = 0.1f;
    low.restitution = 0.05f;
    const CombinedContactSurface mixed = PhysicsMaterial_Combine(high, low);
    Expect(NearlyEqual(mixed.muStatic, 0.3f), "min staticFriction");
    Expect(NearlyEqual(mixed.muDynamic, 0.1f), "min dynamicFriction");
    Expect(NearlyEqual(mixed.restitution, 0.2f), "max restitution of mixed pair");

    const ContactFrictionDecision stick = PhysicsMaterial_DecideFriction(
        0.1f, 1.0f, 1.0f, 0.5f, 0.3f);
    Expect(stick.mode == ContactFrictionMode::StaticStick, "stick when jStick inside static cone");

    const ContactFrictionDecision slip = PhysicsMaterial_DecideFriction(
        2.0f, 1.0f, 1.0f, 0.5f, 0.3f);
    Expect(slip.mode == ContactFrictionMode::DynamicClamp, "dynamic clamp when jStick exceeds static cone");
    Expect(NearlyEqual(slip.jFriction, 0.3f), "dynamic clamp uses muDynamic * |jn|");

    const ContactFrictionDecision rest = PhysicsMaterial_DecideFriction(
        0.01f, 1.0f, 0.0f, 0.5f, 0.3f);
    Expect(rest.mode == ContactFrictionMode::ResidualStick, "residual stick with no normal impulse");

    const ContactFrictionDecision noGrip = PhysicsMaterial_DecideFriction(
        0.01f, 1.0f, 0.0f, 0.0f, 0.0f);
    Expect(noGrip.mode == ContactFrictionMode::Skip, "no residual stick when muStatic is 0");

    const ContactFrictionDecision iceRest = PhysicsMaterial_DecideFriction(
        0.04f, 1.0f, 0.0f, 0.04f, 0.02f);
    Expect(iceRest.mode != ContactFrictionMode::ResidualStick,
        "low-mu ice does not residual-stick when jn is 0");

    if (g_failed != 0) {
        std::printf("%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("all physics material checks passed\n");
    return 0;
}
