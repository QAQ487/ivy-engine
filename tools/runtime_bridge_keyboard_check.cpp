// runtime_bridge_keyboard_check.cpp
// Validates runtime bridge keyboard injection.

#include "engine/testing/runtime_test_bridge.h"
#include "engine/input/key_logger.h"
#include <iostream>
using namespace ivy::testing;
static void check(bool b, const char* label) {
    if (!b) { std::cerr << "FAIL " << label << std::endl; std::exit(1); }
}
struct Peer {
    HANDLE h = INVALID_HANDLE_VALUE;
    int id = 0;
    explicit Peer(const Bridge& b) {
        const auto deadline = GetTickCount64()+2000;
        do {
            h=CreateFileA(b.PipeName().c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);
            if(h!=INVALID_HANDLE_VALUE) break;
            Sleep(1);
        } while(GetTickCount64()<deadline);
        check(h!=INVALID_HANDLE_VALUE,"connect keyboard peer");
    }
    ~Peer() { close(); }
    void close() { if(h!=INVALID_HANDLE_VALUE) {CloseHandle(h); h=INVALID_HANDLE_VALUE;} }
    Json request(const char* verb, Json payload=Json::object()) {
        payload["protocolVersion"]=1;payload["requestId"]=++id;payload["runId"]="keyboard-check";payload["verb"]=verb;
        const auto frame=payload.dump()+"\n";
        DWORD n=0;
        check(WriteFile(h,frame.data(),static_cast<DWORD>(frame.size()),&n,nullptr)!=FALSE,"send keyboard request");
        std::string result; char c=0;
        while(ReadFile(h,&c,1,&n,nullptr) && n==1 && c!='\n') result+=c;
        check(c=='\n',"read keyboard reply");
        return Json::parse(result);
    }
};
int main() {
    SetEnvironmentVariableA("IVY_RUNTIME_TEST_BRIDGE","1");
    SetEnvironmentVariableA("IVY_RUNTIME_TEST_RUN_ID","keyboard-check");
    KeyLogger_Initialize(); KeyLogger_Update(0);
    Bridge b;check(b.StartFromEnvironment(),"keyboard bridge starts");
    b.PublishAndTakeQuit(true,true,"fixture");
    Peer p(b);
    Keyboard_ProcessMessage(WM_KEYDOWN,'W',0);KeyLogger_Update(.001);
    check(p.request("keyboard",{{"keys",{"W"}},{"updates",1}}).value("ok",false),"N=1 accepted");
    b.PollAndApplyInput(true);KeyLogger_Update(.002);
    check(!KeyLogger_IsPressed(KK_W) && KeyLogger_IsRelease(KK_W),"neutral preparation isolates preheld same key history");
    b.FinishInputUpdate();
    b.PollAndApplyInput(true);KeyLogger_Update(1.0/60);
    check(KeyLogger_IsPressed(KK_W) && KeyLogger_IsTrigger(KK_W),"N=1 visible to real KeyLogger");
    b.FinishInputUpdate();
    check(p.request("status")["keyboard"]["status"]=="active","not complete before release Update");
    b.PollAndApplyInput(true);KeyLogger_Update(2.0/60);
    check(!KeyLogger_IsPressed(KK_W) && KeyLogger_IsRelease(KK_W),"N=1 release reaches KeyLogger");
    check(p.request("status")["keyboard"]["status"]=="releasing","release pending until Update finished");
    check(p.request("keyboard",{{"keys",{"A"}},{"updates",1}}).value("error","")=="queue_full","release Update still owns slot");
    b.FinishInputUpdate();
    check(p.request("status")["keyboard"]["status"]=="completed","completed after release Update");
    b.PollAndApplyInput(true);KeyLogger_Update(3.0/60);b.FinishInputUpdate();
    Keyboard_ProcessMessage(WM_KEYDOWN, 'D', 0);
    check(p.request("keyboard",{{"keys",{"W"}},{"updates",3}}).value("ok",false),"N=3 accepted");
    b.PollAndApplyInput(true);KeyLogger_Update(3.5/60);b.FinishInputUpdate();
    b.PollAndApplyInput(true);KeyLogger_Update(4.0/60);
    check(KeyLogger_IsPressed(KK_W) && !KeyLogger_IsPressed(KK_D),"physical held before injection isolated");
    Keyboard_ProcessMessage(WM_KEYDOWN, 'A', 0);
    Keyboard_ProcessMessage(WM_KEYUP, 'W', 0);
    check(KeyLogger_IsPressed(KK_W) && !KeyLogger_IsPressed(KK_A),"physical messages cannot alter effective test state");
    b.FinishInputUpdate();
    for(int i=0;i<2;++i) {
        b.PollAndApplyInput(true);KeyLogger_Update((5+i)/60.0);
        check(KeyLogger_IsPressed(KK_W) && !KeyLogger_IsTrigger(KK_W),"N>1 hold no retrigger");
        check(KeyLogger_GetHoldTime(KK_W)>1.0/60,"real hold time accumulates");
        b.FinishInputUpdate();
    }
    Keyboard_ProcessMessage(WM_ACTIVATEAPP,FALSE,0);
    Keyboard_ProcessMessage(WM_KEYDOWN,'W',0);
    b.PollAndApplyInput(true);KeyLogger_Update(7.0/60);
    check(!KeyLogger_IsPressed(KK_W) && KeyLogger_IsRelease(KK_W),"release isolated even when physical W is held");
    b.FinishInputUpdate();
    check(p.request("status")["keyboard"]["status"]=="completed","N>1 completed");
    b.PollAndApplyInput(true);KeyLogger_Update(8.0/60);b.FinishInputUpdate();
    check(KeyLogger_IsPressed(KK_W),"latest physical state resumes only after release Update");
    Keyboard_ProcessMessage(WM_KEYUP,'W',0);KeyLogger_Update(9.0/60);
    check(p.request("keyboard",{{"keys",{"W"}},{"updates",120}}).value("ok",false),"queued disconnect accepted");
    p.close();
    Peer p2(b);p2.id=p.id;
    check(p2.request("status")["keyboard"]["status"]=="cancelled","disconnect before consume cancels queue");
    b.PollAndApplyInput(true);KeyLogger_Update(10.0/60);b.FinishInputUpdate();
    check(!KeyLogger_IsPressed(KK_W),"disconnected queued key never applied");
    check(p2.request("keyboard",{{"keys",{"SPACE","UP"}},{"updates",120}}).value("ok",false),"reconnect operation accepted");
    b.PollAndApplyInput(true);KeyLogger_Update(10.5/60);b.FinishInputUpdate();
    b.PollAndApplyInput(true);KeyLogger_Update(11.0/60);b.FinishInputUpdate();
    check(KeyLogger_IsPressed(KK_SPACE) && KeyLogger_IsTrigger(KK_UP),"multi-key active");
    check(p2.request("keyboard",{{"keys",{"W"}},{"updates",1}}).value("error","")=="queue_full","bounded active slot");
    p2.close();Peer p3(b);p3.id=p2.id;
    auto cancelled=p3.request("status")["keyboard"];
    check(cancelled["status"]=="active" && cancelled["cancelRequested"]==true,"active disconnect awaits owner release");
    b.PollAndApplyInput(true);KeyLogger_Update(12.0/60);
    check(KeyLogger_IsRelease(KK_SPACE) && !KeyLogger_IsPressed(KK_UP),"active disconnect released on next Update");
    b.FinishInputUpdate();
    check(p3.request("status")["keyboard"]["status"]=="cancelled","cancelled after release consumed");
    check(p3.request("keyboard",{{"keys",{"W"}},{"updates",120}}).value("ok",false),"stalled operation accepted");
    b.PollAndApplyInput(true);KeyLogger_Update(12.5/60);b.FinishInputUpdate();
    b.PollAndApplyInput(true);KeyLogger_Update(13.0/60);b.FinishInputUpdate();
    const auto stall=GetTickCount64();
    Json stalled;
    do { Sleep(100);stalled=p3.request("status")["keyboard"]; } while(GetTickCount64()-stall<5200);
    check(stalled["status"]=="active" && stalled["cancelRequested"]==true,"wall deadline requests cancellation without false completion while Update stalled");
    b.PollAndApplyInput(true);KeyLogger_Update(14.0/60);
    check(KeyLogger_IsRelease(KK_W),"deadline released on resumed Update");b.FinishInputUpdate();
    check(p3.request("status")["keyboard"]["status"]=="cancelled","deadline cancellation acknowledged");
    check(p3.request("keyboard",{{"keys",{"RIGHT"}},{"updates",120}}).value("ok",false),"shutdown operation accepted");
    b.PollAndApplyInput(true);KeyLogger_Update(14.5/60);b.FinishInputUpdate();
    b.PollAndApplyInput(true);KeyLogger_Update(15.0/60);b.FinishInputUpdate();
    const auto stop=GetTickCount64();b.Stop();
    check(GetTickCount64()-stop<1000 && !Keyboard_IsInjectionActive() && !Keyboard_IsKeyDown(KK_RIGHT),"bounded Stop releases active synthetic state");
    p3.close();
    check(b.StartFromEnvironment(),"restart instance cleanly");b.PublishAndTakeQuit(true,true,"fixture");
    Peer p4(b);
    check(p4.request("status")["keyboard"]["status"]=="idle","no static state survives restart");
    check(p4.request("keyboard",{{"keys",{"DOWN"}},{"updates",120}}).value("ok",false),"restart accepts fresh operation");
    b.PollAndApplyInput(true);KeyLogger_Update(16.0/60);b.FinishInputUpdate();
    b.PollAndApplyInput(true);KeyLogger_Update(17.0/60);b.FinishInputUpdate();
    b.PollAndApplyInput(false);KeyLogger_Update(18.0/60);
    check(KeyLogger_IsRelease(KK_DOWN),"not-playing transition releases active keys");b.FinishInputUpdate();
    check(p4.request("status")["keyboard"]["status"]=="cancelled","not-playing active cancellation acknowledged");
    b.Stop();
    std::cout << "PASS keyboard N=1/N=3 trigger/hold/release physical isolation disconnect/reconnect deadline/shutdown/restart\n";
}
