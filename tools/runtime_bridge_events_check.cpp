// Events verb + recorder protocol checks (deterministic recorder APIs; runtime positive is callback).
#include "engine/testing/runtime_test_bridge.h"
#include <iostream>
#include <string>

using namespace ivy::testing;

static void check(bool b, const char* label) {
    if (!b) {
        std::cerr << "FAIL " << label << std::endl;
        std::exit(1);
    }
}

static Json eventsReq(Protocol& p, int id) {
    return p.Request(
        "{\"protocolVersion\":1,\"requestId\":" + std::to_string(id)
        + ",\"runId\":\"" + p.RunId() + "\",\"verb\":\"events\"}");
}

int main() {
    RuntimeTestEvents_SetEnabled(true);
    RuntimeTestEvents_Reset();

    Protocol p("events-test", 4242, 1);
    p.Publish(true, true, "checkpoint_fixture.json");

    auto status = p.Request(
        R"({"protocolVersion":1,"requestId":1,"runId":"events-test","verb":"status"})");
    check(status.value("ok", false) == true, "status ok before events");
    bool hasEvents = false;
    for (const auto& cap : status["capabilities"]) {
        if (cap == "events") hasEvents = true;
    }
    check(hasEvents, "capabilities include events");

    auto r = eventsReq(p, 2);
    check(r.value("ok", false) == true, "events ok true");
    check(r.value("runId", "") == "events-test", "events runId identity");
    check(r.value("requestId", 0) == 2, "events requestId identity");
    check(r.contains("events") && r["events"].is_array() && r["events"].empty(), "events empty array");
    check(r.value("latestSequence", -1) == 0, "latestSequence 0");
    check(r.value("dropped", -1) == 0, "dropped 0");

    auto badExtra = p.Request(
        R"({"protocolVersion":1,"requestId":3,"runId":"events-test","verb":"events","inject":true})");
    check(badExtra.value("ok", true) == false && badExtra.value("error", "") == "invalid_request",
        "events rejects extra keys");

    auto badWrite = p.Request(
        R"({"protocolVersion":1,"requestId":4,"runId":"events-test","verb":"events","events":[{"sequence":9}]})");
    check(badWrite.value("ok", true) == false, "events rejects client-supplied events payload");

    check(RuntimeTestEvents_RecordCheckpoint("Checkpoint_North", "Player", 1), "record first checkpoint");
    check(!RuntimeTestEvents_RecordCheckpoint("Checkpoint_North", "Player", 2), "reject non-1 score");
    check(!RuntimeTestEvents_RecordCheckpoint("", "Player", 1), "reject empty checkpoint");
    check(!RuntimeTestEvents_RecordCheckpoint("Checkpoint_North", nullptr, 1), "reject null actor");
    p.Publish(true, true, "checkpoint_fixture.json");
    r = eventsReq(p, 5);
    check(r["events"].size() == 1, "one published event");
    check(r["events"][0]["type"] == "checkpoint_reached", "type checkpoint_reached");
    check(r["events"][0]["checkpoint"] == "Checkpoint_North", "checkpoint name");
    check(r["events"][0]["actor"] == "Player", "actor Player");
    check(r["events"][0]["score"] == 1, "score 1");
    check(r["events"][0]["sequence"] == 1, "sequence 1");
    check(r.value("latestSequence", 0) == 1, "latestSequence 1");

    check(RuntimeTestEvents_RecordCheckpoint("Checkpoint_North", "Player", 1), "recorder allows second record call");
    p.Publish(true, true, "checkpoint_fixture.json");
    r = eventsReq(p, 6);
    check(r["events"].size() == 2, "two recorder entries without script latch");
    check(r.value("latestSequence", 0) == 2, "latestSequence 2");

    RuntimeTestEvents_Reset();
    p.Publish(true, true, "checkpoint_fixture.json");
    r = eventsReq(p, 7);
    check(r["events"].empty() && r.value("latestSequence", -1) == 0, "reset clears published snapshot");

    RuntimeTestEvents_SetEnabled(false);
    check(!RuntimeTestEvents_RecordCheckpoint("Checkpoint_North", "Player", 1), "disabled recorder rejects");
    RuntimeTestEvents_SetEnabled(true);
    RuntimeTestEvents_Reset();
    for (int i = 0; i < 64; ++i) {
        check(RuntimeTestEvents_RecordCheckpoint("Checkpoint_North", "Player", 1), "fill capacity");
    }
    check(!RuntimeTestEvents_RecordCheckpoint("Checkpoint_North", "Player", 1), "capacity overflow rejects");
    p.Publish(true, true, "checkpoint_fixture.json");
    r = eventsReq(p, 8);
    check(r["events"].size() == 64, "bounded 64 events");
    check(r.value("dropped", 0) == 1, "dropped metadata");

    RuntimeTestEvents_SetEnabled(false);
    std::cout << "PASS events verb + recorder bounds/reset/optin checks\n";
    return 0;
}
