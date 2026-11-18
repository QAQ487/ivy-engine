// runtime_bridge_editor_play_check.cpp
// Protocol validation for Editor Play-mode bridge (Phase 7):
// play_enter/play_exit/load_scene/set_setting, whitelist, and Edit-mode gating.

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

static bool CapHas(const Json& caps, const char* name) {
    if (!caps.is_array()) return false;
    for (const auto& c : caps) {
        if (c.is_string() && c.get<std::string>() == name) return true;
    }
    return false;
}

int main() {
    // --- Pure whitelist helpers ---
    check(IsRuntimeTestBridgeSettingKey("frustumCullingEnabled"), "whitelist frustum");
    check(IsRuntimeTestBridgeSettingKey("shadowCascadeCullingEnabled"), "whitelist shadow");
    check(IsRuntimeTestBridgeSettingKey("spriteCullingEnabled"), "whitelist sprite");
    check(!IsRuntimeTestBridgeSettingKey("fogEnabled"), "reject non-whitelist key");
    check(IsRuntimeTestBridgeNumericSettingKey("lodBias"), "numeric whitelist lodBias");
    check(IsRuntimeTestBridgeNumericSettingKey("effectCullRadius"), "numeric whitelist effectCullRadius");
    check(IsRuntimeTestBridgeNumericSettingKey("lodForceLevel"), "numeric whitelist lodForceLevel");
    check(!IsRuntimeTestBridgeNumericSettingKey("lodEnabled"), "bool key is not numeric");
    check(!IsRuntimeTestBridgeSettingKey("lodBias"), "numeric key is not bool");
    {
        double n = 0.0;
        std::string nerr;
        check(ParseRuntimeTestBridgeNumericSettingValue(Json(2.5), n, nerr) && n == 2.5, "numeric accepts double");
        check(ParseRuntimeTestBridgeNumericSettingValue(Json(3), n, nerr) && n == 3.0, "numeric accepts int");
        check(!ParseRuntimeTestBridgeNumericSettingValue(Json(true), n, nerr), "numeric rejects bool");
        check(!ParseRuntimeTestBridgeNumericSettingValue(Json("1"), n, nerr), "numeric rejects string");
    }
    check(!IsRuntimeTestBridgeSettingKey(""), "reject empty key");

    bool outBool = false;
    std::string err;
    check(ParseRuntimeTestBridgeSettingValue(Json(true), outBool, err) && outBool && err.empty(),
          "bool true ok");
    check(ParseRuntimeTestBridgeSettingValue(Json(false), outBool, err) && !outBool && err.empty(),
          "bool false ok");
    check(!ParseRuntimeTestBridgeSettingValue(Json(1), outBool, err), "reject int value");
    check(!ParseRuntimeTestBridgeSettingValue(Json("true"), outBool, err), "reject string value");

    check(ParseRuntimeTestBridgePlayMode(nullptr) == EditorPlayModeKind::Viewport,
          "default play mode viewport");
    check(ParseRuntimeTestBridgePlayMode("viewport") == EditorPlayModeKind::Viewport, "mode viewport");
    check(ParseRuntimeTestBridgePlayMode("fullscreen") == EditorPlayModeKind::Fullscreen, "mode fullscreen");
    check(ParseRuntimeTestBridgePlayMode("bogus") == EditorPlayModeKind::Invalid, "mode invalid");

    // --- Editor host: status/capabilities ---
    Protocol ed("editor7", 7, 1, BridgeHostKind::Editor);
    auto hello = ed.Request(R"({"protocolVersion":1,"requestId":1,"runId":"editor7","verb":"hello"})");
    check(hello["ok"] == true, "editor hello ok");
    check(hello["host"] == "editor", "editor hello host");
    check(hello["playMode"] == "none", "editor hello playMode none");
    check(hello["playing"] == false, "editor hello not playing");
    check(CapHas(hello["capabilities"], "play_enter"), "cap play_enter");
    check(CapHas(hello["capabilities"], "play_exit"), "cap play_exit");
    check(CapHas(hello["capabilities"], "load_scene"), "cap load_scene");
    check(CapHas(hello["capabilities"], "set_setting"), "cap set_setting");
    check(CapHas(hello["capabilities"], "observe"), "cap observe still present");

    // Runtime host must not advertise editor-only verbs.
    Protocol rt("runtime7", 8, 1, BridgeHostKind::Runtime);
    auto rtHello = rt.Request(R"({"protocolVersion":1,"requestId":1,"runId":"runtime7","verb":"hello"})");
    check(rtHello["host"] == "runtime", "runtime hello host");
    check(!CapHas(rtHello["capabilities"], "play_enter"), "runtime no play_enter");
    check(!CapHas(rtHello["capabilities"], "load_scene"), "runtime no load_scene");
    check(CapHas(rtHello["capabilities"], "set_setting"), "runtime has set_setting");

    // --- Edit-mode gating (ready scene, not playing) ---
    ed.Publish(true, false, "rom/scene/fixture.json", {}, {}, nullptr, EditorPlayModeKind::None);
    auto gated = ed.Request(R"({"protocolVersion":1,"requestId":2,"runId":"editor7","verb":"observe","target":"Player"})");
    check(gated["ok"] == false && gated["error"] == "not_playing", "observe gated in edit");
    check(gated["requestId"] == 2, "observe gate echoes requestId");

    gated = ed.Request(R"({"protocolVersion":1,"requestId":3,"runId":"editor7","verb":"pause"})");
    check(gated["ok"] == false && gated["error"] == "not_playing" && gated["requestId"] == 3,
          "pause gated in edit");
    gated = ed.Request(R"({"protocolVersion":1,"requestId":4,"runId":"editor7","verb":"keyboard","keys":["W"],"updates":1})");
    check(gated["ok"] == false && gated["error"] == "not_playing" && gated["requestId"] == 4,
          "keyboard gated in edit");
    gated = ed.Request(R"({"protocolVersion":1,"requestId":5,"runId":"editor7","verb":"capture"})");
    check(gated["ok"] == false && gated["error"] == "not_playing" && gated["requestId"] == 5,
          "capture gated in edit");
    gated = ed.Request(R"({"protocolVersion":1,"requestId":6,"runId":"editor7","verb":"set_transform","object":"A","position":[1,2,3]})");
    check(gated["ok"] == false && gated["error"] == "not_playing" && gated["requestId"] == 6,
          "set_transform gated in edit");

    // Runtime ready&&!playing keeps not_ready (no semantic change).
    rt.Publish(true, false, "fixture");
    auto rtGate = rt.Request(R"({"protocolVersion":1,"requestId":2,"runId":"runtime7","verb":"pause"})");
    check(rtGate["ok"] == false && rtGate["error"] == "not_ready", "runtime keeps not_ready");

    // --- play_enter / play_exit validation + queue ---
    auto badEnter = ed.Request(R"({"protocolVersion":1,"requestId":7,"runId":"editor7","verb":"play_enter","mode":"bogus"})");
    check(badEnter["error"] == "invalid_request" && badEnter["requestId"] == 7, "play_enter rejects bad mode");
    badEnter = ed.Request(R"({"protocolVersion":1,"requestId":8,"runId":"editor7","verb":"play_enter","extra":1})");
    check(badEnter["error"] == "invalid_request", "play_enter rejects unknown fields");

    auto enter = ed.Request(R"({"protocolVersion":1,"requestId":9,"runId":"editor7","verb":"play_enter"})");
    check(enter["ok"] == true && enter["status"] == "queued", "play_enter queues");
    EditorOpRequest eop{};
    check(ed.HasPendingEditorOp(eop) && eop.kind == EditorOpKind::PlayEnter, "pending play_enter");
    check(eop.playMode == EditorPlayModeKind::Viewport, "default viewport");
    ed.CompleteEditorOp(eop.requestId, true, "");
    ed.Publish(true, true, "rom/scene/fixture.json", {}, {}, nullptr, EditorPlayModeKind::Viewport);

    auto status = ed.Request(R"({"protocolVersion":1,"requestId":10,"runId":"editor7","verb":"status"})");
    check(status["playing"] == true && status["playMode"] == "viewport", "status after play_enter");

    auto again = ed.Request(R"({"protocolVersion":1,"requestId":11,"runId":"editor7","verb":"play_enter","mode":"fullscreen"})");
    check(again["ok"] == false && again["error"] == "already_playing" && again["requestId"] == 11,
          "play_enter rejects already playing");

    auto exit = ed.Request(R"({"protocolVersion":1,"requestId":12,"runId":"editor7","verb":"play_exit"})");
    check(exit["ok"] == true && exit["status"] == "queued", "play_exit queues");
    check(ed.HasPendingEditorOp(eop) && eop.kind == EditorOpKind::PlayExit, "pending play_exit");
    ed.CompleteEditorOp(eop.requestId, true, "");
    ed.Publish(true, false, "rom/scene/fixture.json", {}, {}, nullptr, EditorPlayModeKind::None);

    auto exitAgain = ed.Request(R"({"protocolVersion":1,"requestId":13,"runId":"editor7","verb":"play_exit"})");
    check(exitAgain["ok"] == true && exitAgain.value("playing", true) == false, "play_exit idempotent");

    // Runtime rejects editor-only verbs.
    auto unsup = rt.Request(R"({"protocolVersion":1,"requestId":3,"runId":"runtime7","verb":"play_enter"})");
    check(unsup["error"] == "unsupported_host" && unsup["requestId"] == 3, "runtime play_enter unsupported");
    unsup = rt.Request(R"({"protocolVersion":1,"requestId":4,"runId":"runtime7","verb":"load_scene","scene":"rom/scene/a.json"})");
    check(unsup["error"] == "unsupported_host", "runtime load_scene unsupported");

    // --- load_scene ---
    auto badLoad = ed.Request(R"({"protocolVersion":1,"requestId":14,"runId":"editor7","verb":"load_scene"})");
    check(badLoad["error"] == "invalid_request", "load_scene requires scene");
    badLoad = ed.Request(R"({"protocolVersion":1,"requestId":15,"runId":"editor7","verb":"load_scene","scene":"../etc/passwd.json"})");
    check(badLoad["error"] == "invalid_request", "load_scene rejects traversal");
    auto load = ed.Request(R"({"protocolVersion":1,"requestId":16,"runId":"editor7","verb":"load_scene","scene":"rom/scene/fixture.json"})");
    check(load["ok"] == true && load["status"] == "queued", "load_scene queues");
    check(ed.HasPendingEditorOp(eop) && eop.kind == EditorOpKind::LoadScene, "pending load_scene");
    check(eop.scenePath == "rom/scene/fixture.json", "load_scene path normalized");
    ed.CompleteEditorOp(eop.requestId, true, "");

    // --- set_setting ---
    auto badSet = ed.Request(R"({"protocolVersion":1,"requestId":17,"runId":"editor7","verb":"set_setting","key":"fogEnabled","value":true})");
    check(badSet["error"] == "invalid_request" && badSet["requestId"] == 17, "set_setting rejects unknown key");
    badSet = ed.Request(R"({"protocolVersion":1,"requestId":18,"runId":"editor7","verb":"set_setting","key":"frustumCullingEnabled","value":1})");
    check(badSet["error"] == "invalid_request", "set_setting rejects non-bool");
    badSet = ed.Request(R"({"protocolVersion":1,"requestId":19,"runId":"editor7","verb":"set_setting","key":"frustumCullingEnabled"})");
    check(badSet["error"] == "invalid_request", "set_setting requires value");

    auto set = ed.Request(R"({"protocolVersion":1,"requestId":20,"runId":"editor7","verb":"set_setting","key":"frustumCullingEnabled","value":false})");
    check(set["ok"] == true && set["status"] == "queued", "set_setting queues");
    check(ed.HasPendingEditorOp(eop) && eop.kind == EditorOpKind::SetSetting, "pending set_setting");
    check(eop.settingKey == "frustumCullingEnabled" && eop.settingBoolValue == false, "set_setting payload");
    ed.CompleteEditorOp(eop.requestId, true, "");

    // Numeric settings: number accepted, bool rejected, bool keys still reject numbers.
    badSet = ed.Request(R"({"protocolVersion":1,"requestId":21,"runId":"editor7","verb":"set_setting","key":"lodBias","value":true})");
    check(badSet["error"] == "invalid_request", "numeric setting rejects bool");
    badSet = ed.Request(R"({"protocolVersion":1,"requestId":22,"runId":"editor7","verb":"set_setting","key":"lodBias","value":"2"})");
    check(badSet["error"] == "invalid_request", "numeric setting rejects string");
    auto numSet = ed.Request(R"({"protocolVersion":1,"requestId":23,"runId":"editor7","verb":"set_setting","key":"lodBias","value":2.5})");
    check(numSet["ok"] == true && numSet["status"] == "queued", "numeric set_setting queues");
    check(ed.HasPendingEditorOp(eop) && eop.kind == EditorOpKind::SetSetting, "pending numeric set_setting");
    check(eop.settingKey == "lodBias" && eop.settingIsNumber && eop.settingNumberValue == 2.5, "numeric payload");
    ed.CompleteEditorOp(eop.requestId, true, "");

    // set_setting also on runtime host.
    auto rtSet = rt.Request(R"({"protocolVersion":1,"requestId":5,"runId":"runtime7","verb":"set_setting","key":"spriteCullingEnabled","value":true})");
    check(rtSet["ok"] == true && rtSet["status"] == "queued", "runtime set_setting queues");
    check(rt.HasPendingEditorOp(eop) && eop.kind == EditorOpKind::SetSetting, "runtime pending set_setting");

    // Play mode: observe works again.
    ed.Publish(true, true, "rom/scene/fixture.json", {
        ObservedTarget{"Player", 1, 2, 3}
    }, {}, nullptr, EditorPlayModeKind::Fullscreen);
    auto obs = ed.Request(R"({"protocolVersion":1,"requestId":24,"runId":"editor7","verb":"observe","target":"Player"})");
    check(obs["ok"] == true && obs["found"] == true, "observe ok while playing");
    auto st = ed.Request(R"({"protocolVersion":1,"requestId":25,"runId":"editor7","verb":"status"})");
    check(st["playMode"] == "fullscreen", "status reports fullscreen");

    // --- Editor bridge quit policy (StopPlay then DestroyWindow; skip unsaved) ---
    {
        const auto playingQuit = PlanEditorBridgeHostQuit(true);
        check(playingQuit.stopPlayFirst, "quit while playing stops Play first");
        check(playingQuit.postDestroyWindow, "quit posts DestroyWindow");
        check(playingQuit.skipUnsavedConfirm, "quit skips unsaved confirm");
        const auto editQuit = PlanEditorBridgeHostQuit(false);
        check(!editQuit.stopPlayFirst, "quit in Edit mode does not StopPlay");
        check(editQuit.postDestroyWindow && editQuit.skipUnsavedConfirm,
              "quit in Edit mode still destroys without unsaved popup");
    }

    // Protocol quit still ACKs with receipt (host process exit is WinMain-side).
    auto quitAck = ed.Request(R"({"protocolVersion":1,"requestId":26,"runId":"editor7","verb":"quit"})");
    check(quitAck["ok"] == true && quitAck["requiresReceipt"] == true, "editor quit ack");
    check(quitAck["requestId"] == 26, "quit echoes requestId");

    std::cout << "PASS Phase 7 editor play-mode bridge protocol\n";
    return 0;
}
