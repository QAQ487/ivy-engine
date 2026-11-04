// runtime_bridge_observe_pose_check.cpp
// Offline check: observe snapshots prefer Transform-component pose over legacy fields.

#include "engine/testing/runtime_test_bridge.h"
#include <cmath>
#include <iostream>

using namespace ivy::testing;

static void check(bool b, const char* label) {
    if (!b) {
        std::cerr << "FAIL " << label << std::endl;
        std::exit(1);
    }
}

static bool ApproxEq(float a, float b) {
    return std::fabs(a - b) < 1e-5f;
}

int main() {
    const ObservePose legacy{
        {1.0f, 2.0f, 3.0f},
        {10.0f, 20.0f, 30.0f},
        {1.0f, 1.0f, 1.0f}};
    const ObservePose component{
        {9.0f, 8.0f, 7.0f},
        {40.0f, 50.0f, 60.0f},
        {2.0f, 3.0f, 4.0f}};

    const auto without = ResolveObservedPose(false, legacy, component);
    check(ApproxEq(without.position.x, 1.0f) && ApproxEq(without.position.y, 2.0f)
              && ApproxEq(without.position.z, 3.0f),
          "without Transform falls back to legacy position");
    check(ApproxEq(without.rotation.y, 20.0f), "without Transform falls back to legacy rotation");
    check(ApproxEq(without.scale.x, 1.0f), "without Transform falls back to legacy scale");

    const auto with = ResolveObservedPose(true, legacy, component);
    check(ApproxEq(with.position.x, 9.0f) && ApproxEq(with.position.y, 8.0f)
              && ApproxEq(with.position.z, 7.0f),
          "with Transform prefers component position (not stale legacy)");
    check(ApproxEq(with.rotation.x, 40.0f) && ApproxEq(with.rotation.y, 50.0f)
              && ApproxEq(with.rotation.z, 60.0f),
          "with Transform prefers component rotation");
    check(ApproxEq(with.scale.x, 2.0f) && ApproxEq(with.scale.y, 3.0f)
              && ApproxEq(with.scale.z, 4.0f),
          "with Transform prefers component scale");

    // Simulate WinMain snapshot publish after a script writes Transform.localPosition.
    ObservedTarget stale{};
    stale.name = "Player";
    AssignObservedPose(stale, legacy);
    Protocol p("observe-pose", 7, 1);
    p.Publish(true, true, "fixture", {stale}, {});
    auto before = p.Request(
        R"({"protocolVersion":1,"requestId":1,"runId":"observe-pose","verb":"observe","target":"Player"})");
    check(before["ok"] == true && before["found"] == true, "observe before component change ok");
    check(before["position"]["x"] == 1.0f, "observe initially reports legacy position");

    ObservedTarget live{};
    live.name = "Player";
    AssignObservedPose(live, ResolveObservedPose(true, legacy, component));
    p.Publish(true, true, "fixture", {live}, {});
    auto after = p.Request(
        R"({"protocolVersion":1,"requestId":2,"runId":"observe-pose","verb":"observe","target":"Player"})");
    check(after["ok"] == true && after["found"] == true, "observe after component change ok");
    check(after["position"]["x"] == 9.0f && after["position"]["y"] == 8.0f
              && after["position"]["z"] == 7.0f,
          "observe reflects Transform component position change");
    check(after["rotation"]["y"] == 50.0f, "observe reflects Transform component rotation change");
    check(after["scale"]["z"] == 4.0f, "observe reflects Transform component scale change");

    std::cout << "PASS observe prefers Transform component pose over stale legacy fields\n";
}
