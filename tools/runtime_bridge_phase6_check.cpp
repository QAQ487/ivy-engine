// runtime_bridge_phase6_check.cpp
// Protocol validation for list_objects, pause/resume/step, perf, and observe extensions.

#include "engine/testing/runtime_test_bridge.h"
#include <cmath>
#include <iostream>
#include <windows.h>
using namespace ivy::testing;
static void check(bool b, const char* label) {
    if (!b) { std::cerr << "FAIL " << label << std::endl; std::exit(1); }
}

int main() {
    Protocol p("phase6", 42, 1);
    p.Publish(true, true, "rom/scene/fixture.json", {
        ObservedTarget{
            "Player", 1.0f, 2.0f, 3.0f, 0.1f, 0.0f, 0.2f,
            true, 10.0f, 20.0f, 30.0f, 0.0f, 0.0f, 0.0f, 1.0f,
            true, 1.0f, 1.0f, 1.0f,
            true, 0.0f, 1.0f, 0.0f,
            true, false
        }
    }, {
        ListedObject{"Player", {"SceneTransform", "Rigidbody", "visual"}},
        ListedObject{"Ground", {"SceneTransform", "BoxCollider"}},
    });

    auto obs = p.Request(R"({"protocolVersion":1,"requestId":1,"runId":"phase6","verb":"observe","target":"Player"})");
    check(obs["ok"] == true && obs["found"] == true, "observe ok");
    check(obs.contains("position") && obs.contains("velocity"), "observe keeps legacy fields");
    check(obs.contains("rotation") && obs["rotation"]["y"] == 20.0f, "observe rotation euler");
    check(obs.contains("rotationQuaternion") && obs["rotationQuaternion"].contains("w"), "observe quaternion");
    check(obs.contains("scale") && obs.contains("angularVelocity"), "observe scale and angularVelocity");
    check(obs.contains("rigidbody") && obs["rigidbody"]["kinematic"] == false, "observe rigidbody kinematic");
    check(!obs.contains("sleeping") && !obs.contains("contactCount"), "omit unavailable sleep/contact fields");

    auto badObs = p.Request(R"({"protocolVersion":1,"requestId":2,"runId":"phase6","verb":"observe","target":"Player","extra":1})");
    check(badObs.value("error","") == "invalid_request", "observe rejects unknown fields");

    auto listed = p.Request(R"({"protocolVersion":1,"requestId":3,"runId":"phase6","verb":"list_objects","offset":0,"limit":1})");
    check(listed["ok"] == true, "list_objects ok");
    check(listed["objects"].size() == 1 && listed["total"] == 2, "list_objects pagination");
    check(listed["truncated"] == true, "list_objects truncated when more remain");
    check(listed["objects"][0]["name"] == "Player", "list_objects name");
    check(listed["objects"][0]["components"].is_array(), "list_objects components");

    auto badList = p.Request(R"({"protocolVersion":1,"requestId":4,"runId":"phase6","verb":"list_objects","limit":0})");
    check(badList.value("error","") == "invalid_request", "list_objects rejects limit 0");
    badList = p.Request(R"({"protocolVersion":1,"requestId":5,"runId":"phase6","verb":"list_objects","limit":65})");
    check(badList.value("error","") == "invalid_request", "list_objects rejects oversize limit");
    badList = p.Request(R"({"protocolVersion":1,"requestId":6,"runId":"phase6","verb":"list_objects","unknown":true})");
    check(badList.value("error","") == "invalid_request", "list_objects rejects unknown fields");

    Protocol cold("cold", 1, 1);
    auto notReady = cold.Request(R"({"protocolVersion":1,"requestId":1,"runId":"cold","verb":"list_objects"})");
    check(notReady.value("error","") == "not_ready", "list_objects rejected when not ready");
    cold.Publish(true, false, "fixture");
    notReady = cold.Request(R"({"protocolVersion":1,"requestId":2,"runId":"cold","verb":"pause"})");
    check(notReady.value("error","") == "not_ready", "pause rejected when not playing");

    auto stepEarly = p.Request(R"({"protocolVersion":1,"requestId":7,"runId":"phase6","verb":"step","frames":3})");
    check(stepEarly.value("error","") == "not_paused", "step rejected while not paused");

    auto paused = p.Request(R"({"protocolVersion":1,"requestId":8,"runId":"phase6","verb":"pause"})");
    check(paused["ok"] == true && paused["paused"] == true, "pause ok");
    auto status = p.Request(R"({"protocolVersion":1,"requestId":9,"runId":"phase6","verb":"status"})");
    check(status["paused"] == true && status["state"] == "paused", "status reports paused");
    check(status["capabilities"].is_array(), "status capabilities present");
    bool hasStep = false, hasPerf = false, hasList = false;
    for (const auto& cap : status["capabilities"]) {
        if (cap == "step") hasStep = true;
        if (cap == "perf") hasPerf = true;
        if (cap == "list_objects") hasList = true;
    }
    check(hasStep && hasPerf && hasList, "new capabilities advertised");

    auto badStep = p.Request(R"({"protocolVersion":1,"requestId":10,"runId":"phase6","verb":"step","frames":0})");
    check(badStep.value("error","") == "invalid_request", "step rejects frames 0");
    badStep = p.Request(R"({"protocolVersion":1,"requestId":11,"runId":"phase6","verb":"step","frames":601})");
    check(badStep.value("error","") == "invalid_request", "step rejects frames 601");

    auto stepped = p.Request(R"({"protocolVersion":1,"requestId":12,"runId":"phase6","verb":"step","frames":3})");
    check(stepped["ok"] == true && stepped["pendingSteps"] == 3, "step queues frames");
    check(p.ShouldRunSimTick() && p.ShouldRunSimTick() && p.ShouldRunSimTick(), "step consumes three ticks");
    check(!p.ShouldRunSimTick(), "paused with no pending steps freezes sim");
    p.NoteSimFrame();
    p.NotePhysicsTick();

    auto resumed = p.Request(R"({"protocolVersion":1,"requestId":13,"runId":"phase6","verb":"resume"})");
    check(resumed["ok"] == true && resumed["paused"] == false, "resume ok");
    check(p.ShouldRunSimTick(), "resume restores continuous sim");

    for (int i = 0; i < 10; ++i)
        p.RecordPerfSample(PerfSample{16.0f + (i % 3), 4.0f, 5.0f, 2.0f});
    auto perf = p.Request(R"({"protocolVersion":1,"requestId":14,"runId":"phase6","verb":"perf","frames":8})");
    check(perf["ok"] == true && perf["frames"] == 8, "perf returns requested window");
    check(perf.contains("frameMs") && perf["frameMs"].contains("p95"), "perf frameMs stats");
    check(perf.contains("updateMs") && perf.contains("drawMs") && perf.contains("presentMs"), "perf stage stats");
    check(perf.contains("fps") && perf["fps"].contains("avg"), "perf fps");
    auto badPerf = p.Request(R"({"protocolVersion":1,"requestId":15,"runId":"phase6","verb":"perf","frames":0})");
    check(badPerf.value("error","") == "invalid_request", "perf rejects frames 0");
    badPerf = p.Request(R"({"protocolVersion":1,"requestId":16,"runId":"phase6","verb":"perf","frames":601})");
    check(badPerf.value("error","") == "invalid_request", "perf rejects frames 601");

    std::cout << "PASS Phase 6 observe extensions + list_objects/pause/resume/step/perf protocol\n";
    return 0;
}
