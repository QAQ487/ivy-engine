// runtime_bridge_world_manip_check.cpp
// Protocol validation for world-manipulation verbs (reset_scene / set_transform /
// set_velocity / apply_impulse / apply_torque_impulse).

#include "engine/testing/runtime_test_bridge.h"
#include <cmath>
#include <iostream>
#include <string>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
using namespace ivy::testing;

static void check(bool b, const char* label) {
    if (!b) { std::cerr << "FAIL " << label << std::endl; std::exit(1); }
}

static Json baseFor(const char* runId, std::int64_t id, const char* verb) {
    return Json{{"protocolVersion",1},{"requestId",id},{"runId",runId},{"verb",verb}};
}
static Json base(std::int64_t id, const char* verb) {
    return baseFor("world", id, verb);
}

int main() {
    Protocol p("world", 7, 1);
    p.Publish(true, true, "rom/scene/fixture.json");

    auto status = p.Request(base(1, "status").dump());
    check(status["ok"] == true, "status ok");
    bool hasReset = false, hasSetXf = false, hasImpulse = false;
    for (const auto& cap : status["capabilities"]) {
        if (cap == "reset_scene") hasReset = true;
        if (cap == "set_transform") hasSetXf = true;
        if (cap == "apply_impulse") hasImpulse = true;
    }
    check(hasReset && hasSetXf && hasImpulse, "world manip capabilities advertised");

    // --- reset_scene schema ---
    auto okReset = p.Request(base(2, "reset_scene").dump());
    check(okReset["ok"] == true && okReset["status"] == "queued", "reset_scene queues");
    WorldOpRequest pending{};
    check(p.HasPendingWorldOp(pending), "reset_scene pending");
    check(pending.kind == WorldOpKind::ResetScene, "reset_scene kind");
    p.CompleteWorldOp(2, true, "");
    status = p.Request(base(3, "status").dump());
    check(status["worldOp"]["status"] == "completed", "reset_scene completed via status");

    auto badScene = base(4, "reset_scene");
    badScene["scene"] = "../rom/scene/x.json";
    check(p.Request(badScene.dump()).value("error","") == "invalid_request", "reset_scene rejects ..");
    badScene = base(5, "reset_scene");
    badScene["scene"] = "rom/scene/nope.txt";
    check(p.Request(badScene.dump()).value("error","") == "invalid_request", "reset_scene rejects non-json");
    badScene = base(6, "reset_scene");
    badScene["extra"] = 1;
    check(p.Request(badScene.dump()).value("error","") == "invalid_request", "reset_scene rejects unknown keys");
    auto goodScene = base(7, "reset_scene");
    goodScene["scene"] = "rom/scene/scene_runtime_bridge_world_manip.json";
    check(p.Request(goodScene.dump())["ok"] == true, "reset_scene accepts validated path");
    check(p.HasPendingWorldOp(pending) && pending.scenePath.find("rom/scene/") == 0, "reset_scene stores path");
    p.CompleteWorldOp(7, true, "");

    // --- set_transform schema ---
    auto xf = base(8, "set_transform");
    xf["object"] = "DynamicBox";
    xf["position"] = Json::array({1.0, 2.0, 3.0});
    auto queued = p.Request(xf.dump());
    check(queued["ok"] == true && queued["verb"] == "set_transform", "set_transform queues");
    check(p.HasPendingWorldOp(pending), "set_transform pending");
    check(pending.hasPosition && pending.posX == 1.0f && pending.object == "DynamicBox", "set_transform fields");
    check(pending.zeroVelocity == true, "set_transform default zeroVelocity");
    p.CompleteWorldOp(8, true, "");

    auto badXf = base(9, "set_transform");
    badXf["object"] = "DynamicBox";
    check(p.Request(badXf.dump()).value("error","") == "invalid_request", "set_transform requires a pose field");
    badXf = base(10, "set_transform");
    badXf["object"] = "DynamicBox";
    badXf["position"] = Json::array({1.0, 2.0});
    check(p.Request(badXf.dump()).value("error","") == "invalid_request", "set_transform rejects short position");
    badXf = base(11, "set_transform");
    badXf["object"] = "DynamicBox";
    badXf["position"] = Json::array({1.0, 2.0, HUGE_VALF});
    check(p.Request(badXf.dump()).value("error","") == "invalid_request", "set_transform rejects non-finite");
    badXf = base(12, "set_transform");
    badXf["object"] = "bad name!";
    badXf["position"] = Json::array({0.0, 0.0, 0.0});
    check(p.Request(badXf.dump()).value("error","") == "invalid_request", "set_transform rejects bad object name");
    badXf = base(13, "set_transform");
    badXf["object"] = "DynamicBox";
    badXf["rotation"] = Json::array({0.0, 0.0, 0.0, 0.0});
    check(p.Request(badXf.dump()).value("error","") == "invalid_request", "set_transform rejects zero quat");
    auto quatXf = base(14, "set_transform");
    quatXf["object"] = "DynamicBox";
    quatXf["rotation"] = Json::array({0.0, 0.0, 0.0, 1.0});
    quatXf["zeroVelocity"] = false;
    check(p.Request(quatXf.dump())["ok"] == true, "set_transform accepts quaternion");
    check(p.HasPendingWorldOp(pending) && pending.hasRotationQuat && pending.zeroVelocity == false, "quat + zeroVelocity flag");
    p.CompleteWorldOp(14, true, "");

    // --- set_velocity / impulses ---
    auto vel = base(15, "set_velocity");
    vel["object"] = "DynamicBox";
    vel["linear"] = Json::array({1.0, 0.0, 0.0});
    check(p.Request(vel.dump())["ok"] == true, "set_velocity queues");
    p.CompleteWorldOp(15, true, "");

    auto badVel = base(16, "set_velocity");
    badVel["object"] = "DynamicBox";
    check(p.Request(badVel.dump()).value("error","") == "invalid_request", "set_velocity requires linear or angular");

    auto impulse = base(17, "apply_impulse");
    impulse["object"] = "DynamicBox";
    impulse["impulse"] = Json::array({5.0, 0.0, 0.0});
    impulse["point"] = Json::array({0.0, 1.0, 0.0});
    check(p.Request(impulse.dump())["ok"] == true, "apply_impulse queues");
    check(p.HasPendingWorldOp(pending) && pending.hasPoint, "apply_impulse point");
    p.CompleteWorldOp(17, false, "not_dynamic");
    status = p.Request(base(18, "status").dump());
    check(status["worldOp"]["status"] == "failed" && status["worldOp"]["error"] == "not_dynamic",
        "failed world op surfaces error");

    auto torque = base(19, "apply_torque_impulse");
    torque["object"] = "DynamicBox";
    torque["torqueImpulse"] = Json::array({0.0, 1.0, 0.0});
    check(p.Request(torque.dump())["ok"] == true, "apply_torque_impulse queues");
    // queue_full while still queued
    auto again = base(20, "apply_torque_impulse");
    again["object"] = "DynamicBox";
    again["torqueImpulse"] = Json::array({0.0, 1.0, 0.0});
    check(p.Request(again.dump()).value("error","") == "queue_full", "world op queue_full");
    p.CompleteWorldOp(19, true, "");

    auto unknownKey = base(21, "apply_impulse");
    unknownKey["object"] = "DynamicBox";
    unknownKey["impulse"] = Json::array({1.0, 0.0, 0.0});
    unknownKey["force"] = Json::array({1.0, 0.0, 0.0});
    check(p.Request(unknownKey.dump()).value("error","") == "invalid_request", "apply_impulse rejects unknown keys");

    // Match list_objects/capture/pause: world verbs need ready+playing. Cold
    // Protocol runId must match the request (else run_mismatch masks not_ready).
    Protocol cold("cold", 1, 1);
    auto missingObject = baseFor("cold", 1, "set_transform");
    check(cold.Request(missingObject.dump()).value("error","") == "invalid_request",
        "set_transform without object invalid");
    auto nr = baseFor("cold", 2, "reset_scene");
    check(cold.Request(nr.dump()).value("error","") == "not_ready", "reset_scene not_ready when cold");
    cold.Publish(true, false, "fixture");
    auto notPlaying = baseFor("cold", 3, "apply_impulse");
    notPlaying["object"] = "DynamicBox";
    notPlaying["impulse"] = Json::array({1.0, 0.0, 0.0});
    check(cold.Request(notPlaying.dump()).value("error","") == "not_ready",
        "apply_impulse not_ready when not playing");

    // Quit must be accepted while paused (host flushes exit without a sim tick).
    Protocol pausedQuit("paused-quit", 9, 1);
    pausedQuit.Publish(true, true, "rom/scene/fixture.json");
    check(pausedQuit.Request(baseFor("paused-quit", 1, "pause").dump())["ok"] == true, "pause before quit");
    check(pausedQuit.IsPaused(), "paused for quit");
    check(!pausedQuit.ShouldRunSimTick(), "paused freezes sim ticks");
    auto quitWhilePaused = pausedQuit.Request(baseFor("paused-quit", 2, "quit").dump());
    check(quitWhilePaused["ok"] == true && quitWhilePaused["requiresReceipt"] == true,
        "quit accepted while paused");
    pausedQuit.ConfirmQuitReceipt();
    check(pausedQuit.TakeQuit(), "quit receipt ready while paused");

    std::cout << "PASS world manip reset_scene/set_transform/set_velocity/impulse protocol\n";
    return 0;
}
