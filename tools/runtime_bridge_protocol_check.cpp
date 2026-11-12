// runtime_bridge_protocol_check.cpp
// Validates runtime bridge framing/protocol parsing.

#include "engine/testing/runtime_test_bridge.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#include <iostream>
#include "engine/input/controller.h"
#include "engine/input/key_logger.h"
#include <stdexcept>
#include <vector>
using namespace ivy::testing;
void check(bool b, const char* message) { if (!b) { std::cerr << "FAIL " << message << std::endl; std::exit(1); } }
int main(int argc, char** argv) {
#ifdef _WIN32
    if(argc>1) {
        KeyLogger_Initialize();
        Controller_Initialize();
        Bridge server;
        check(server.StartFromEnvironment(), "harness bridge starts");
        std::cout << server.PipeName() << std::endl;
        const bool cancel = std::string(argv[1]) == "--cancel";
        const auto deadline=GetTickCount64()+(cancel ? 250 : 15000);
        while(GetTickCount64()<deadline) {
            if(server.PublishAndTakeQuit(true,true,"fixture.json")) { server.Stop(); return 0; }
            if(std::string(argv[1])!="--stall") {
                server.PollAndApplyInput(true);
                Controller_Update();
                RuntimeTestControllerProbe_SampleFromGetters();
                KeyLogger_Update(static_cast<double>(GetTickCount64())/1000.0);
                server.FinishInputUpdate();
            }
            Sleep(2);
        }
        server.Stop();
        return cancel ? 0 : 2;
    }
#else
    (void)argc; (void)argv;
#endif
    check(RemainingFrameTimeout(2000, 1999) == 1, "one millisecond remains");
    check(RemainingFrameTimeout(2000, 2000) == 0, "deadline reached");
    check(RemainingFrameTimeout(2000, 2001) == 0, "expired deadline never wraps to INFINITE");
    check(RemainingFrameTimeout(2000, 0) == 2000, "bounded frame timeout");
    check(InputInjectDeadlineMs(1) == kInputInjectDeadlineBaseMs + kInputInjectDeadlinePerUpdateMs,
          "keyboard deadline base + one update");
    check(InputInjectDeadlineMs(kInputInjectMaxUpdates) == kInputInjectDeadlineMaxMs,
          "keyboard deadline at max updates equals documented upper bound");
    check(InputInjectDeadlineMs(kInputInjectMaxUpdates) >
              kInputInjectDeadlineBaseMs + static_cast<std::uint64_t>(kInputInjectMaxUpdates) * 40,
          "scaled deadline covers Debug ~23 FPS max hold (not fixed 5000ms)");
    check(InputInjectDeadlineMs(0) == InputInjectDeadlineMs(1), "deadline clamps updates below 1");
    check(InputInjectDeadlineMs(kInputInjectMaxUpdates + 50) == kInputInjectDeadlineMaxMs,
          "deadline clamps updates above max");
    {
        Protocol deadlineProto("deadline-scale", 123, 7);
        deadlineProto.Publish(true, true, "fixture");
        const auto t0 = GetTickCount64();
        auto queued = deadlineProto.Request(
            R"({"protocolVersion":1,"requestId":1,"runId":"deadline-scale","verb":"keyboard","keys":["W"],"updates":120})");
        check(queued.value("ok", false), "max-updates keyboard accepted for deadline probe");
        // Former fixed 5000ms budget cancelled ~23 FPS holds; scaled budget must survive 5.2s.
        deadlineProto.BeginInput(true, t0 + 5200);
        deadlineProto.EndInput();
        auto mid = deadlineProto.Request(
            R"({"protocolVersion":1,"requestId":2,"runId":"deadline-scale","verb":"status"})");
        check(mid["keyboard"]["status"] == "active" && mid["keyboard"]["cancelRequested"] == false,
              "5.2s elapsed does not cancel updates=120 under scaled deadline");
        deadlineProto.BeginInput(true, t0 + InputInjectDeadlineMs(120) + 1);
        auto late = deadlineProto.Request(
            R"({"protocolVersion":1,"requestId":3,"runId":"deadline-scale","verb":"status"})");
        check(late["keyboard"]["status"] == "releasing" && late["keyboard"]["cancelRequested"] == true,
              "past scaled deadline begins release with cancelRequested");
        deadlineProto.EndInput();
        auto done = deadlineProto.Request(
            R"({"protocolVersion":1,"requestId":4,"runId":"deadline-scale","verb":"status"})");
        check(done["keyboard"]["status"] == "cancelled", "scaled deadline cancellation acknowledged");
    }
    Protocol boundary("boundary", 123, 7);
    boundary.Publish(true, true, std::string(1023, 'a') + "\xC3\xA9");
    auto boundaryReply = boundary.Request(R"({"protocolVersion":1,"requestId":1,"runId":"boundary","verb":"status"})");
    bool utf8Safe = true;
    try { (void)boundaryReply.dump(); } catch(const std::exception&) { utf8Safe=false; }
    check(utf8Safe && boundaryReply["scene"] == std::string(1023, 'a'), "UTF-8 truncation preserves complete codepoints");
    Protocol p("test-run", 123, 7);
    auto r = p.Request(R"({"protocolVersion":1,"requestId":1,"runId":"test-run","verb":"hello"})");
    check(r["ok"] == true, "hello succeeds");
    check(r["pid"] == 123 && r["sessionId"] == 7 && r["runId"] == "test-run", "exact identity");
    check(r["state"] == "loading" && r["ready"] == false, "startup is not ready");
    r = p.Request(R"({"protocolVersion":2,"requestId":2,"runId":"test-run","verb":"hello"})");
    check(r.value("error", "") == "unsupported_version", "reject unknown version");
    r = p.Request("{");
    check(r.value("error", "") == "malformed_json", "reject malformed JSON");
    for (const auto& bad : {"[]", "null", "{}", "{\"requestId\":true}"}) {
        r = p.Request(bad);
        check(r.value("error", "") == "invalid_request", "reject invalid schema");
    }
    r = p.Request(std::string(4097, ' '));
    check(r.value("error", "") == "message_too_large", "reject oversize");
    r = p.Request(R"({"protocolVersion":1,"requestId":3,"runId":"wrong","verb":"hello"})");
    check(r.value("error", "") == "run_mismatch", "reject wrong run");
    r = p.Request(R"({"protocolVersion":1,"requestId":4,"runId":"test-run","verb":"act"})");
    check(r.value("error", "") == "unknown_verb", "reject out-of-scope verbs");
    r = p.Request(R"({"protocolVersion":1,"requestId":1,"runId":"test-run","verb":"hello"})");
    check(r.value("error", "") == "duplicate_request_id", "reject duplicate or stale ID");
    p.Publish(true, true, "rom/scene/test.json");
    r = p.Request(R"({"protocolVersion":1,"requestId":5,"runId":"test-run","verb":"status"})");
    check(r["ready"] == true && r["playing"] == true && r["scene"] == "rom/scene/test.json", "copied main-thread snapshot");
    Protocol keyboard("keyboard", 123, 7);
    keyboard.Publish(true, true, "fixture");
    auto keyReply = keyboard.Request(R"({"protocolVersion":1,"requestId":1,"runId":"keyboard","verb":"keyboard","keys":["W"],"updates":1})");
    check(keyReply.value("ok", false), "keyboard extended payload accepted");
    for(const auto& badUpdates : {Json(0),Json(121),Json(true),Json(1.5),Json(4294967297ULL),Json(-1)}) {
        Protocol invalid("keyboard",123,7); invalid.Publish(true,true,"fixture");
        auto bad=Json{{"protocolVersion",1},{"requestId",1},{"runId","keyboard"},{"verb","keyboard"},{"keys",{"W"}},{"updates",badUpdates}};
        check(invalid.Request(bad.dump()).value("error","")=="invalid_request","reject invalid or overflowing updates before narrowing");
    }
    for(const auto& badKeys : {Json::array(),Json::array({"W","W"}),Json::array({"NOTAKEY"}),Json::array({"LEFTWINDOWS"}),Json::array({"RIGHTWINDOWS"}),Json::array({true}),Json("W")}) {
        Protocol invalid("keyboard",123,7); invalid.Publish(true,true,"fixture");
        auto bad=Json{{"protocolVersion",1},{"requestId",1},{"runId","keyboard"},{"verb","keyboard"},{"keys",badKeys},{"updates",1}};
        check(!invalid.Request(bad.dump()).value("ok",true),"reject duplicate/unknown/invalid keys");
    }
    for(const auto& forbidden : {Json::array({"LALT","F4"}),Json::array({"RALT","F4"}),Json::array({"F4","LALT","E"})}) {
        Protocol invalid("keyboard",123,7); invalid.Publish(true,true,"fixture");
        auto bad=Json{{"protocolVersion",1},{"requestId",1},{"runId","keyboard"},{"verb","keyboard"},{"keys",forbidden},{"updates",1}};
        check(!invalid.Request(bad.dump()).value("ok",true),"reject ALT+F4 system combo");
    }
    for(const auto& goodKeys : {Json::array({"ESCAPE"}),Json::array({"ENTER"}),Json::array({"TAB"}),Json::array({"E"}),Json::array({"1"}),Json::array({"LCTRL","E"}),Json::array({"LSHIFT","TAB"}),Json::array({"LALT","TAB"})}) {
        Protocol valid("keyboard",123,7); valid.Publish(true,true,"fixture");
        auto good=Json{{"protocolVersion",1},{"requestId",1},{"runId","keyboard"},{"verb","keyboard"},{"keys",goodKeys},{"updates",1}};
        check(valid.Request(good.dump()).value("ok",false),"accept expanded key table and modifier combos");
    }
    Protocol policy("policy",123,7);
    Json input{{"protocolVersion",1},{"requestId",1},{"runId","policy"},{"verb","keyboard"},{"keys",{"W"}},{"updates",120}};
    check(policy.Request(input.dump()).value("error","")=="not_ready","input rejected before ready");
    policy.Publish(true,false,"fixture"); input["requestId"]=2;
    check(policy.Request(input.dump()).value("error","")=="not_ready","input rejected when not playing");
    policy.Publish(true,true,"fixture");input["requestId"]=3;input["extra"]=true;
    check(policy.Request(input.dump()).value("error","")=="invalid_request","extra payload rejected");
    input.erase("extra");input["protocolVersion"]=true;
    check(policy.Request(input.dump()).value("error","")=="invalid_request","boolean version rejected");
    input["protocolVersion"]=2;
    check(policy.Request(input.dump()).value("error","")=="unsupported_version","keyboard version checked");
    input["protocolVersion"]=1;input["runId"]="wrong";
    check(policy.Request(input.dump()).value("error","")=="run_mismatch","keyboard run checked");
    input["runId"]="policy";
    check(policy.Request(input.dump()).value("ok",false),"schema/version/run errors did not consume ID");
    input["requestId"]=4;
    check(policy.Request(input.dump()).value("error","")=="queue_full","one queued input slot");
    policy.CancelKeyboard();
    input["requestId"]=5;input["keys"]=Json::array({"W","A","S","D","SPACE","UP","DOWN","LEFT","RIGHT"});
    check(policy.Request(input.dump()).value("ok",false),"max nine unique keys still supported");
    policy.BeginInput(false,GetTickCount64());
    auto cancelled=policy.Request(R"({"protocolVersion":1,"requestId":6,"runId":"policy","verb":"status"})");
    check(cancelled["keyboard"]["status"]=="cancelled","not-playing cancels queued input");
    check(r["capabilities"] == Json::array({"hello","status","quit","keyboard","observe","capture","mouse","controller","events","controller_probe","list_objects","pause","resume","step","perf","reset_scene","set_transform","set_velocity","apply_impulse","apply_torque_impulse"}), "capabilities include observe, capture, mouse, controller, events, controller_probe, list_objects, pause, resume, step, perf, and world manip verbs");

    // Absolute cursor (optional x,y in backbuffer pixels). Stubs report 1280x720.
    {
        Protocol mouseAbs("mouse-abs", 123, 7);
        mouseAbs.Publish(true, true, "fixture");
        auto okAbs = Json{{"protocolVersion",1},{"requestId",1},{"runId","mouse-abs"},{"verb","mouse"},
            {"dx",0},{"dy",0},{"buttons",Json::array({"LEFT"})},{"updates",1},{"x",640},{"y",360}};
        check(mouseAbs.Request(okAbs.dump()).value("ok", false), "accept absolute x,y within backbuffer");
        auto onlyX = Json{{"protocolVersion",1},{"requestId",2},{"runId","mouse-abs"},{"verb","mouse"},
            {"dx",0},{"dy",0},{"buttons",Json::array()},{"updates",1},{"x",10}};
        check(mouseAbs.Request(onlyX.dump()).value("error","")=="invalid_request", "reject x without y");
        Protocol mouseAbs2("mouse-abs2", 123, 7);
        mouseAbs2.Publish(true, true, "fixture");
        auto onlyY = Json{{"protocolVersion",1},{"requestId",1},{"runId","mouse-abs2"},{"verb","mouse"},
            {"dx",0},{"dy",0},{"buttons",Json::array()},{"updates",1},{"y",10}};
        check(mouseAbs2.Request(onlyY.dump()).value("error","")=="invalid_request", "reject y without x");
        for (const auto& badPair : {
            Json{{"x", -1}, {"y", 0}},
            Json{{"x", 0}, {"y", -1}},
            Json{{"x", 1280}, {"y", 0}},
            Json{{"x", 0}, {"y", 720}},
            Json{{"x", 1.5}, {"y", 0}},
            Json{{"x", true}, {"y", 0}},
            Json{{"x", "10"}, {"y", 0}},
        }) {
            Protocol invalid("mouse-abs-bad", 123, 7);
            invalid.Publish(true, true, "fixture");
            auto bad = Json{{"protocolVersion",1},{"requestId",1},{"runId","mouse-abs-bad"},{"verb","mouse"},
                {"dx",0},{"dy",0},{"buttons",Json::array()},{"updates",1},
                {"x", badPair["x"]}, {"y", badPair["y"]}};
            check(invalid.Request(bad.dump()).value("error","")=="invalid_request",
                "reject non-int or out-of-backbuffer absolute cursor");
        }
        Protocol mouseLife("mouse-life", 123, 7);
        mouseLife.Publish(true, true, "fixture");
        check(mouseLife.Request(Json{{"protocolVersion",1},{"requestId",1},{"runId","mouse-life"},{"verb","mouse"},
            {"dx",5},{"dy",-5},{"buttons",Json::array({"LEFT"})},{"wheel",10},{"updates",1},
            {"x",100},{"y",200}}.dump()).value("ok", false), "accept absolute with wheel");
        int mdx=0,mdy=0,mw=0,ax=0,ay=0; unsigned int mb=0; bool hasAbs=false;
        mouseLife.BeginInput(true, GetTickCount64());
        mouseLife.GetMouseInput(mdx, mdy, mb, mw, hasAbs, ax, ay);
        check(hasAbs && ax==100 && ay==200 && mdx==0 && mb==0, "absolute available during prep; relative neutral");
        mouseLife.EndInput();
        mouseLife.BeginInput(true, GetTickCount64());
        mouseLife.GetMouseInput(mdx, mdy, mb, mw, hasAbs, ax, ay);
        check(hasAbs && ax==100 && ay==200 && mdx==5 && mdy==-5 && (mb & 1) && mw==10,
            "absolute + relative during active hold");
        mouseLife.EndInput();
        mouseLife.BeginInput(true, GetTickCount64());
        mouseLife.GetMouseInput(mdx, mdy, mb, mw, hasAbs, ax, ay);
        check(hasAbs && ax==100 && ay==200 && mdx==0 && mb==0, "absolute persists through release; relative neutral");
        mouseLife.EndInput();
        mouseLife.BeginInput(true, GetTickCount64());
        mouseLife.GetMouseInput(mdx, mdy, mb, mw, hasAbs, ax, ay);
        check(!hasAbs && !mouseLife.MouseIsolating(), "absolute cleared after mouse op completes");
        mouseLife.EndInput();
        Protocol mouseRel("mouse-rel", 123, 7);
        mouseRel.Publish(true, true, "fixture");
        check(mouseRel.Request(Json{{"protocolVersion",1},{"requestId",1},{"runId","mouse-rel"},{"verb","mouse"},
            {"dx",1},{"dy",1},{"buttons",Json::array()},{"updates",1}}.dump()).value("ok", false),
            "relative-only mouse still accepted");
        mouseRel.BeginInput(true, GetTickCount64());
        mouseRel.GetMouseInput(mdx, mdy, mb, mw, hasAbs, ax, ay);
        check(!hasAbs, "omitting x,y leaves absolute override off");
        mouseRel.EndInput();
    }

    // durationMs wall-clock mouse hold (exclusive with updates).
    check(InputInjectDeadlineFromDurationMs(1) == kInputInjectDeadlineBaseMs + 1,
          "duration deadline base + one ms");
    check(InputInjectDeadlineFromDurationMs(kInputInjectMaxDurationMs) ==
              kInputInjectDeadlineBaseMs + static_cast<std::uint64_t>(kInputInjectMaxDurationMs),
          "duration deadline at max durationMs");
    check(InputInjectDeadlineFromDurationMs(0) == InputInjectDeadlineFromDurationMs(1),
          "duration deadline clamps below 1");
    check(InputInjectDeadlineFromDurationMs(kInputInjectMaxDurationMs + 50) ==
              InputInjectDeadlineFromDurationMs(kInputInjectMaxDurationMs),
          "duration deadline clamps above max");
    {
        Protocol mouseDur("mouse-dur", 123, 7);
        mouseDur.Publish(true, true, "fixture");
        auto both = Json{{"protocolVersion",1},{"requestId",1},{"runId","mouse-dur"},{"verb","mouse"},
            {"dx",0},{"dy",0},{"buttons",Json::array({"LEFT"})},{"updates",1},{"durationMs",100}};
        check(mouseDur.Request(both.dump()).value("error","")=="invalid_request",
              "reject updates and durationMs together");
        Protocol mouseDurNeither("mouse-dur-neither", 123, 7);
        mouseDurNeither.Publish(true, true, "fixture");
        auto neither = Json{{"protocolVersion",1},{"requestId",1},{"runId","mouse-dur-neither"},{"verb","mouse"},
            {"dx",0},{"dy",0},{"buttons",Json::array({"LEFT"})}};
        check(mouseDurNeither.Request(neither.dump()).value("error","")=="invalid_request",
              "reject mouse without updates or durationMs");
        for (const auto& badDuration : {Json(0), Json(30001), Json(true), Json(1.5), Json(-1), Json("100")}) {
            Protocol invalid("mouse-dur-bad", 123, 7);
            invalid.Publish(true, true, "fixture");
            auto bad = Json{{"protocolVersion",1},{"requestId",1},{"runId","mouse-dur-bad"},{"verb","mouse"},
                {"dx",0},{"dy",0},{"buttons",Json::array()},{"durationMs",badDuration}};
            check(invalid.Request(bad.dump()).value("error","")=="invalid_request",
                  "reject non-int or out-of-range durationMs");
        }
        Protocol mouseDurOk("mouse-dur-ok", 123, 7);
        mouseDurOk.Publish(true, true, "fixture");
        check(mouseDurOk.Request(Json{{"protocolVersion",1},{"requestId",1},{"runId","mouse-dur-ok"},{"verb","mouse"},
            {"dx",0},{"dy",0},{"buttons",Json::array({"LEFT"})},{"durationMs",3500},
            {"x",189},{"y",527}}.dump()).value("ok", false),
            "accept durationMs LEFT hold with absolute cursor");

        // Hold remains pressed until elapsed >= durationMs (injectable now).
        // Use GetTickCount64()-based times so ExpireMouse deadline (set at Request) does not fire first.
        Protocol mouseHold("mouse-hold", 123, 7);
        mouseHold.Publish(true, true, "fixture");
        check(mouseHold.Request(Json{{"protocolVersion",1},{"requestId",1},{"runId","mouse-hold"},{"verb","mouse"},
            {"dx",0},{"dy",0},{"buttons",Json::array({"LEFT"})},{"durationMs",100},
            {"x",189},{"y",527}}.dump()).value("ok", false), "queue durationMs=100 hold");
        const std::uint64_t tHold = GetTickCount64();
        int hdx=0,hdy=0,hw=0,hx=0,hy=0; unsigned int hb=0; bool hAbs=false;
        mouseHold.BeginInput(true, tHold); // Queued -> Preparing
        mouseHold.GetMouseInput(hdx, hdy, hb, hw, hAbs, hx, hy);
        check(hAbs && hx==189 && hy==527 && hb==0, "prep frame: absolute on, buttons neutral");
        mouseHold.EndInput(); // Preparing -> Active
        mouseHold.BeginInput(true, tHold); // Active; hold clock starts
        mouseHold.GetMouseInput(hdx, hdy, hb, hw, hAbs, hx, hy);
        check(hAbs && hx==189 && hy==527 && (hb & 1u), "active: LEFT held at duration start");
        mouseHold.EndInput();
        mouseHold.BeginInput(true, tHold + 99);
        mouseHold.GetMouseInput(hdx, hdy, hb, hw, hAbs, hx, hy);
        check((hb & 1u) && mouseHold.MouseIsolating(), "still holding at durationMs-1");
        auto midHold = mouseHold.Request(
            R"({"protocolVersion":1,"requestId":2,"runId":"mouse-hold","verb":"status"})");
        check(midHold["mouse"]["status"] == "active" && midHold["mouse"]["cancelRequested"] == false,
              "status active before duration elapses");
        mouseHold.EndInput();
        mouseHold.BeginInput(true, tHold + 100);
        mouseHold.GetMouseInput(hdx, hdy, hb, hw, hAbs, hx, hy);
        check(hb==0 && hAbs && hx==189 && hy==527, "release at elapsed==durationMs; absolute kept");
        auto releasing = mouseHold.Request(
            R"({"protocolVersion":1,"requestId":3,"runId":"mouse-hold","verb":"status"})");
        check(releasing["mouse"]["status"] == "releasing", "status releasing when duration elapses");
        mouseHold.EndInput();
        auto doneHold = mouseHold.Request(
            R"({"protocolVersion":1,"requestId":4,"runId":"mouse-hold","verb":"status"})");
        check(doneHold["mouse"]["status"] == "completed", "duration hold completes without cancel");

        // Deadline scales with durationMs (base + duration); long holds are not cancelled early.
        Protocol mouseDeadline("mouse-deadline", 123, 7);
        mouseDeadline.Publish(true, true, "fixture");
        const auto t0 = GetTickCount64();
        check(mouseDeadline.Request(Json{{"protocolVersion",1},{"requestId",1},{"runId","mouse-deadline"},
            {"verb","mouse"},{"dx",0},{"dy",0},{"buttons",Json::array({"LEFT"})},
            {"durationMs",30000}}.dump()).value("ok", false), "max durationMs accepted for deadline probe");
        mouseDeadline.BeginInput(true, t0);
        mouseDeadline.EndInput();
        mouseDeadline.BeginInput(true, t0); // start hold clock
        mouseDeadline.EndInput();
        mouseDeadline.BeginInput(true, t0 + 5200);
        auto midDl = mouseDeadline.Request(
            R"({"protocolVersion":1,"requestId":2,"runId":"mouse-deadline","verb":"status"})");
        check(midDl["mouse"]["status"] == "active" && midDl["mouse"]["cancelRequested"] == false,
              "5.2s elapsed does not cancel durationMs=30000 under scaled deadline");
        mouseDeadline.EndInput();
        mouseDeadline.BeginInput(true, t0 + InputInjectDeadlineFromDurationMs(30000) + 1);
        auto lateDl = mouseDeadline.Request(
            R"({"protocolVersion":1,"requestId":3,"runId":"mouse-deadline","verb":"status"})");
        check(lateDl["mouse"]["status"] == "releasing" && lateDl["mouse"]["cancelRequested"] == true,
              "past duration deadline begins release with cancelRequested");
        mouseDeadline.EndInput();
        auto doneDl = mouseDeadline.Request(
            R"({"protocolVersion":1,"requestId":4,"runId":"mouse-deadline","verb":"status"})");
        check(doneDl["mouse"]["status"] == "cancelled", "duration deadline cancellation acknowledged");

        // updates-mode mouse also uses scaled InputInjectDeadlineMs (parity with keyboard).
        Protocol mouseUpdDl("mouse-upd-dl", 123, 7);
        mouseUpdDl.Publish(true, true, "fixture");
        const auto tUpd = GetTickCount64();
        check(mouseUpdDl.Request(Json{{"protocolVersion",1},{"requestId",1},{"runId","mouse-upd-dl"},
            {"verb","mouse"},{"dx",0},{"dy",0},{"buttons",Json::array()},{"updates",120}}.dump())
            .value("ok", false), "updates=120 mouse accepted for deadline probe");
        mouseUpdDl.BeginInput(true, tUpd + 5200);
        mouseUpdDl.EndInput();
        auto midUpd = mouseUpdDl.Request(
            R"({"protocolVersion":1,"requestId":2,"runId":"mouse-upd-dl","verb":"status"})");
        check(midUpd["mouse"]["status"] == "active" && midUpd["mouse"]["cancelRequested"] == false,
              "5.2s elapsed does not cancel updates=120 mouse under scaled deadline");
        mouseUpdDl.BeginInput(true, tUpd + InputInjectDeadlineMs(120) + 1);
        auto lateUpd = mouseUpdDl.Request(
            R"({"protocolVersion":1,"requestId":3,"runId":"mouse-upd-dl","verb":"status"})");
        check(lateUpd["mouse"]["status"] == "releasing" && lateUpd["mouse"]["cancelRequested"] == true,
              "past updates-scaled mouse deadline begins release with cancelRequested");
        mouseUpdDl.EndInput();
    }

    r = p.Request(R"({"protocolVersion":1,"requestId":6,"runId":"test-run","verb":"quit"})");
    check(r.value("requiresReceipt", false), "quit requires receipt before main-thread exit");
    check(!p.TakeQuit(), "no quit before receipt");
    r = p.Request(R"({"protocolVersion":1,"requestId":7,"runId":"test-run","verb":"quit"})");
    check(r.value("error", "") == "queue_full", "bounded one quit slot");
    p.ConfirmQuitReceipt();
    check(p.TakeQuit() && !p.TakeQuit(), "exactly one main-thread quit");
#ifdef _WIN32
    Bridge bridge;
    SetEnvironmentVariableA("IVY_RUNTIME_TEST_BRIDGE", nullptr);
    check(!bridge.StartFromEnvironment(), "off has no worker/endpoint");
    SetEnvironmentVariableA("IVY_RUNTIME_TEST_BRIDGE", "1");
    SetEnvironmentVariableA("IVY_RUNTIME_TEST_RUN_ID", "transport-check");
    check(bridge.StartFromEnvironment(), "explicit opt-in starts endpoint");
    HANDLE peer=CreateFileA(bridge.PipeName().c_str(),GENERIC_READ|GENERIC_WRITE|READ_CONTROL,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
    check(peer!=INVALID_HANDLE_VALUE,"current user can connect");
    PACL dacl=nullptr; PSECURITY_DESCRIPTOR sd=nullptr;
    check(GetSecurityInfo(peer,SE_KERNEL_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,&dacl,nullptr,&sd)==ERROR_SUCCESS,"read actual pipe DACL");
    check(dacl && dacl->AceCount==1,"exactly one allow ACE; no blanket admin/everyone");
    void* ace=nullptr;
    check(GetAce(dacl,0,&ace)!=FALSE && reinterpret_cast<ACE_HEADER*>(ace)->AceType==ACCESS_ALLOWED_ACE_TYPE,"allow ACE");
    HANDLE token=nullptr; check(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)!=FALSE,"read current SID");
    DWORD size=0; GetTokenInformation(token,TokenUser,nullptr,0,&size);
    std::vector<unsigned char> user(size);
    check(GetTokenInformation(token,TokenUser,user.data(),size,&size)!=FALSE,"current token user");
    check(EqualSid(&reinterpret_cast<ACCESS_ALLOWED_ACE*>(ace)->SidStart,reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid)!=FALSE,"DACL is current user SID only");
    CloseHandle(token); LocalFree(sd); CloseHandle(peer);
    const auto start = GetTickCount64();
    bridge.Stop();
    check(GetTickCount64()-start < 1000, "cancel and join with no peer");
    std::cout << "PASS protocol validation/snapshot/quit-slot + opt-in/no-peer cancellation\n";
#else
    std::cout << "PASS protocol validation/snapshot/quit-slot (pipe transport checks skipped on non-Windows)\n";
#endif
}
