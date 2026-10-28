#include "engine/testing/runtime_test_bridge.h"
#define NOMINMAX
#include <windows.h>
#include <aclapi.h>
#include <iostream>
#include "engine/input/key_logger.h"
#include <stdexcept>
#include <vector>
using namespace ivy::testing;
void check(bool b, const char* message) { if (!b) { std::cerr << "FAIL " << message << std::endl; std::exit(1); } }
int main(int argc, char** argv) {
    if(argc>1) {
        KeyLogger_Initialize();
        Bridge server;
        check(server.StartFromEnvironment(), "harness bridge starts");
        std::cout << server.PipeName() << std::endl;
        const bool cancel = std::string(argv[1]) == "--cancel";
        const auto deadline=GetTickCount64()+(cancel ? 250 : 15000);
        while(GetTickCount64()<deadline) {
            if(server.PublishAndTakeQuit(true,true,"fixture.json")) { server.Stop(); return 0; }
            if(std::string(argv[1])!="--stall") {
                server.PollAndApplyInput(true);
                KeyLogger_Update(static_cast<double>(GetTickCount64())/1000.0);
                server.FinishInputUpdate();
            }
            Sleep(2);
        }
        server.Stop();
        return cancel ? 0 : 2;
    }
    check(RemainingFrameTimeout(2000, 1999) == 1, "one millisecond remains");
    check(RemainingFrameTimeout(2000, 2000) == 0, "deadline reached");
    check(RemainingFrameTimeout(2000, 2001) == 0, "expired deadline never wraps to INFINITE");
    check(RemainingFrameTimeout(2000, 0) == 2000, "bounded frame timeout");
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
    for(const auto& badKeys : {Json::array(),Json::array({"W","W"}),Json::array({"ESCAPE"}),Json::array({true}),Json("W")}) {
        Protocol invalid("keyboard",123,7); invalid.Publish(true,true,"fixture");
        auto bad=Json{{"protocolVersion",1},{"requestId",1},{"runId","keyboard"},{"verb","keyboard"},{"keys",badKeys},{"updates",1}};
        check(!invalid.Request(bad.dump()).value("ok",true),"reject duplicate/unknown/invalid keys");
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
    check(policy.Request(input.dump()).value("ok",false),"all nine whitelist keys supported");
    policy.BeginInput(false,GetTickCount64());
    auto cancelled=policy.Request(R"({"protocolVersion":1,"requestId":6,"runId":"policy","verb":"status"})");
    check(cancelled["keyboard"]["status"]=="cancelled","not-playing cancels queued input");
    check(r["capabilities"] == Json::array({"hello","status","quit","keyboard","observe","capture","mouse","controller"}), "capabilities include observe, capture, mouse, and controller");
    r = p.Request(R"({"protocolVersion":1,"requestId":6,"runId":"test-run","verb":"quit"})");
    check(r.value("requiresReceipt", false), "quit requires receipt before main-thread exit");
    check(!p.TakeQuit(), "no quit before receipt");
    r = p.Request(R"({"protocolVersion":1,"requestId":7,"runId":"test-run","verb":"quit"})");
    check(r.value("error", "") == "queue_full", "bounded one quit slot");
    p.ConfirmQuitReceipt();
    check(p.TakeQuit() && !p.TakeQuit(), "exactly one main-thread quit");
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
}
