#include "runtime_test_bridge.h"
#include <cmath>
#include <cstring>
#include <mutex>
#include <set>
#include "engine/input/controller.h"
#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"
#if defined(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
namespace ivy::testing {
namespace {
struct EventsRecorder {
    bool enabled = false;
    std::mutex mutex;
    std::vector<RuntimeTestEvent> events;
    std::int64_t nextSequence = 1;
    std::uint32_t dropped = 0;
};
EventsRecorder g_events;

bool ValidEventName(const char* name) {
    if (!name || !*name) return false;
    const std::size_t n = std::strlen(name);
    if (n == 0 || n > 64) return false;
    for (std::size_t i = 0; i < n; ++i) {
        const char ch = name[i];
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'))
            return false;
    }
    return true;
}
}

void RuntimeTestEvents_SetEnabled(bool enabled) {
    std::lock_guard lock(g_events.mutex);
    g_events.enabled = enabled;
    if (!enabled) {
        g_events.events.clear();
        g_events.nextSequence = 1;
        g_events.dropped = 0;
    }
}
void RuntimeTestEvents_Reset() {
    std::lock_guard lock(g_events.mutex);
    g_events.events.clear();
    g_events.nextSequence = 1;
    g_events.dropped = 0;
}
bool RuntimeTestEvents_IsEnabled() {
    std::lock_guard lock(g_events.mutex);
    return g_events.enabled;
}
bool RuntimeTestEvents_RecordCheckpoint(const char* checkpoint, const char* actor, int score) {
    if (!ValidEventName(checkpoint) || !ValidEventName(actor)) return false;
    if (score != 1) return false;
    std::lock_guard lock(g_events.mutex);
    if (!g_events.enabled) return false;
    if (g_events.events.size() >= kRuntimeTestEventCapacity) {
        ++g_events.dropped;
        return false;
    }
    RuntimeTestEvent ev;
    ev.sequence = g_events.nextSequence++;
    ev.type = "checkpoint_reached";
    ev.checkpoint = checkpoint;
    ev.actor = actor;
    ev.score = score;
    g_events.events.push_back(std::move(ev));
    return true;
}
RuntimeTestEventsSnapshot RuntimeTestEvents_CopySnapshot() {
    std::lock_guard lock(g_events.mutex);
    RuntimeTestEventsSnapshot snap;
    snap.events = g_events.events;
    snap.latestSequence = g_events.nextSequence - 1;
    if (snap.latestSequence < 0) snap.latestSequence = 0;
    snap.dropped = g_events.dropped;
    return snap;
}

std::uint32_t RemainingFrameTimeout(std::uint64_t deadline, std::uint64_t now) noexcept {
    if (now >= deadline) return 0;
    const auto remaining = deadline - now;
    return static_cast<std::uint32_t>(remaining > 2000 ? 2000 : remaining);
}
Protocol::Protocol(std::string runId, unsigned long pid, unsigned long session)
    : runId_(std::move(runId)), pid_(pid), session_(session) {}
void Protocol::Publish(bool ready, bool playing, const std::string& scene, std::vector<ObservedTarget> targets) {
    ready_ = ready; playing_ = playing;
    std::size_t end = scene.size() < 1024 ? scene.size() : 1024;
    if (end < scene.size()) {
        while (end > 0 && (static_cast<unsigned char>(scene[end]) & 0xC0) == 0x80) --end;
    }
    scene_ = scene.substr(0, end);
    targets_ = std::move(targets);
    const auto snap = RuntimeTestEvents_CopySnapshot();
    events_ = snap.events;
    eventsLatestSequence_ = snap.latestSequence;
    eventsDropped_ = snap.dropped;
}
bool Protocol::TakeQuit() { const bool result=quitReady_; quitReady_=false; return result; }
void Protocol::ConfirmQuitReceipt() { if(quitPending_) { quitReady_=true; quitPending_=false; } }
void Protocol::AbandonQuit() { quitPending_=false; }
static const std::set<std::string> kAllowedKeys = {
    "W", "A", "S", "D", "SPACE", "UP", "DOWN", "LEFT", "RIGHT"
};

const char* Protocol::InputStatus() const {
    switch(inputState_) {
    case InputState::Queued:
    case InputState::Preparing: return "queued";
    case InputState::Active: return "active";
    case InputState::Releasing: return "releasing";
    case InputState::Completed: return "completed";
    case InputState::Cancelled: return "cancelled";
    default: return "idle";
    }
}
const char* Protocol::MouseStatus() const {
    switch(mouseState_) {
    case InputState::Queued:
    case InputState::Preparing: return "queued";
    case InputState::Active: return "active";
    case InputState::Releasing: return "releasing";
    case InputState::Completed: return "completed";
    case InputState::Cancelled: return "cancelled";
    default: return "idle";
    }
}
const char* Protocol::ControllerStatus() const {
    return controllerOp_.Status();
}
bool Protocol::KeyboardIsolating() const {
    return inputState_ == InputState::Preparing || inputState_ == InputState::Active || inputState_ == InputState::Releasing;
}
bool Protocol::MouseIsolating() const {
    return mouseState_ == InputState::Preparing || mouseState_ == InputState::Active || mouseState_ == InputState::Releasing;
}
bool Protocol::ControllerIsolating() const {
    return controllerOp_.Isolating();
}
std::vector<std::string> Protocol::InputKeys() const {
    return inputState_ == InputState::Active ? keyboardKeys_ : std::vector<std::string>{};
}
void Protocol::GetMouseInput(int& dx, int& dy, unsigned int& buttonsMask, int& wheel) const {
    if (mouseState_ == InputState::Active) {
        dx = mouseDx_;
        dy = mouseDy_;
        buttonsMask = mouseButtons_;
        wheel = mouseWheel_;
    } else {
        dx = 0;
        dy = 0;
        buttonsMask = 0;
        wheel = 0;
    }
}
void Protocol::GetControllerInput(float& lx, float& ly, float& rx, float& ry, float& lt, float& rt, std::uint16_t& buttons) const {
    const ControllerInjectSample sample = controllerOp_.Sample();
    lx = sample.lx;
    ly = sample.ly;
    rx = sample.rx;
    ry = sample.ry;
    lt = sample.lt;
    rt = sample.rt;
    buttons = sample.buttons;
}
void Protocol::CancelKeyboard() {
    if(inputState_ == InputState::Queued) {
        inputState_ = InputState::Cancelled;
        keyboardUpdatesRemaining_ = 0;
        keyboardKeys_.clear();
    } else if(KeyboardIsolating()) {
        cancelInput_ = true;
    }
}
void Protocol::CancelMouse() {
    if(mouseState_ == InputState::Queued) {
        mouseState_ = InputState::Cancelled;
        mouseUpdatesRemaining_ = 0;
        mouseDx_ = 0; mouseDy_ = 0; mouseButtons_ = 0; mouseWheel_ = 0;
    } else if(MouseIsolating()) {
        cancelMouse_ = true;
    }
}
void Protocol::CancelController() {
    controllerOp_.Cancel();
}
void Protocol::ExpireKeyboard(std::uint64_t now) {
    if(now >= inputDeadline_) CancelKeyboard();
}
void Protocol::ExpireMouse(std::uint64_t now) {
    if(now >= mouseDeadline_) CancelMouse();
}
void Protocol::ExpireController(std::uint64_t now) {
    if(now >= controllerDeadline_) CancelController();
}
void Protocol::BeginInput(bool playing, std::uint64_t now) {
    ExpireKeyboard(now);
    ExpireMouse(now);
    ExpireController(now);
    if(!playing) { CancelKeyboard(); CancelMouse(); }
    if(inputState_ == InputState::Queued) inputState_ = InputState::Preparing;
    if(inputState_ == InputState::Active && (cancelInput_ || keyboardUpdatesRemaining_ == 0)) {
        inputState_ = InputState::Releasing;
        keyboardUpdatesRemaining_ = 0;
        keyboardKeys_.clear();
    }
    if(mouseState_ == InputState::Queued) mouseState_ = InputState::Preparing;
    if(mouseState_ == InputState::Active && (cancelMouse_ || mouseUpdatesRemaining_ == 0)) {
        mouseState_ = InputState::Releasing;
        mouseUpdatesRemaining_ = 0;
        mouseDx_ = 0; mouseDy_ = 0; mouseButtons_ = 0; mouseWheel_ = 0;
    }
    controllerOp_.Begin(playing);
}
void Protocol::EndInput() {
    if(inputState_ == InputState::Preparing) inputState_ = InputState::Active;
    else if(inputState_ == InputState::Active && keyboardUpdatesRemaining_ > 0) --keyboardUpdatesRemaining_;
    else if(inputState_ == InputState::Releasing) {
        inputState_ = cancelInput_ ? InputState::Cancelled : InputState::Completed;
    }
    if(mouseState_ == InputState::Preparing) mouseState_ = InputState::Active;
    else if(mouseState_ == InputState::Active && mouseUpdatesRemaining_ > 0) --mouseUpdatesRemaining_;
    else if(mouseState_ == InputState::Releasing) {
        mouseState_ = cancelMouse_ ? InputState::Cancelled : InputState::Completed;
    }
    controllerOp_.End();
}

bool Protocol::HasPendingCapture(std::int64_t& outId) const {
    if (captureState_.status == CaptureState::Status::Queued) {
        outId = captureState_.requestId;
        return true;
    }
    return false;
}

void Protocol::CompleteCapture(std::int64_t id, bool ok, uint32_t w, uint32_t h, const std::string& basename, const std::string& err) {
    if (captureState_.status == CaptureState::Status::Queued && captureState_.requestId == id) {
        captureState_.status = ok ? CaptureState::Status::Completed : CaptureState::Status::Failed;
        captureState_.width = w;
        captureState_.height = h;
        captureState_.basename = basename;
        captureState_.error = err;
    }
}

Json Protocol::Request(const std::string& frame) {
    ExpireKeyboard(GetTickCount64());
    ExpireMouse(GetTickCount64());
    ExpireController(GetTickCount64());
    auto error = [this](const char* name, Json id=nullptr) -> Json {
        return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",false},{"error",name}};
    };
    if(frame.size()>4096) return error("message_too_large");
    auto q=Json::parse(frame,nullptr,false);
    if(q.is_discarded()) return error("malformed_json");
    if(!q.is_object() || !q.contains("protocolVersion") || !q["protocolVersion"].is_number_integer()
        || !q.contains("requestId") || !q["requestId"].is_number_integer() || q["requestId"]<1 || q["requestId"]>2147483647
        || !q.contains("runId") || !q["runId"].is_string() || !q.contains("verb") || !q["verb"].is_string())
        return error("invalid_request");
    const bool input=q["verb"]=="keyboard";
    const bool mouse=q["verb"]=="mouse";
    const bool controller=q["verb"]=="controller";
    const bool observe=q["verb"]=="observe";
    const bool capture=q["verb"]=="capture";
    const bool events=q["verb"]=="events";
    std::vector<std::string> keys;
    int updates=0;
    int mouseDx=0, mouseDy=0, mouseWheel=0, mouseUpdates=0;
    unsigned int mouseButtonsMask=0;
    ControllerCommand controllerCmd;
    std::string observeTarget;
    if(input) {
        if(q.size()!=6 || !q.contains("keys") || !q["keys"].is_array() || q["keys"].empty() || q["keys"].size()>9
            || !q.contains("updates") || !q["updates"].is_number_integer() || q["updates"]<1 || q["updates"]>120)
            return error("invalid_request");
        std::set<std::string> seen;
        for(const auto& item:q["keys"]) {
            if(!item.is_string()) return error("invalid_request");
            auto name=item.get<std::string>();
            if(!kAllowedKeys.count(name) || !seen.insert(name).second) return error("invalid_request");
            keys.push_back(std::move(name));
        }
        updates=q["updates"].get<int>();
    } else if(mouse) {
        const bool hasWheel = q.contains("wheel");
        const size_t expectedSize = hasWheel ? 9 : 8;
        if(q.size() != expectedSize) return error("invalid_request");
        if(!q.contains("dx") || !q["dx"].is_number_integer() || q["dx"] < -600 || q["dx"] > 600)
            return error("invalid_request");
        if(!q.contains("dy") || !q["dy"].is_number_integer() || q["dy"] < -600 || q["dy"] > 600)
            return error("invalid_request");
        if(!q.contains("buttons") || !q["buttons"].is_array() || q["buttons"].size() > 3)
            return error("invalid_request");
        if(!q.contains("updates") || !q["updates"].is_number_integer() || q["updates"] < 1 || q["updates"] > 120)
            return error("invalid_request");
        if(hasWheel) {
            if(!q["wheel"].is_number_integer() || q["wheel"] < -120 || q["wheel"] > 120)
                return error("invalid_request");
            mouseWheel = q["wheel"].get<int>();
        } else {
            mouseWheel = 0;
        }
        std::set<std::string> seen;
        for(const auto& item : q["buttons"]) {
            if(!item.is_string()) return error("invalid_request");
            auto bname = item.get<std::string>();
            if(!seen.insert(bname).second) return error("invalid_request");
            if(bname == "LEFT") mouseButtonsMask |= MOUSE_BUTTON_LEFT;
            else if(bname == "RIGHT") mouseButtonsMask |= MOUSE_BUTTON_RIGHT;
            else if(bname == "MIDDLE") mouseButtonsMask |= MOUSE_BUTTON_MIDDLE;
            else return error("invalid_request");
        }
        mouseDx = q["dx"].get<int>();
        mouseDy = q["dy"].get<int>();
        mouseUpdates = q["updates"].get<int>();
    } else if(controller) {
        if(ParseControllerCommand(q, controllerCmd) != ControllerParseStatus::Ok) return error("invalid_request");
    } else if(observe) {
        if(q.size()!=5 || !q.contains("target") || !q["target"].is_string())
            return error("invalid_request");
        observeTarget=q["target"].get<std::string>();
        if(observeTarget.empty() || observeTarget.size()>64)
            return error("invalid_request");
        for(char ch:observeTarget) {
            if(!((ch>='a'&&ch<='z') || (ch>='A'&&ch<='Z') || (ch>='0'&&ch<='9') || ch=='_' || ch=='-'))
                return error("invalid_request");
        }
    } else if(capture) {
        if(q.size()!=4) return error("invalid_request");
    } else if(events) {
        if(q.size()!=4) return error("invalid_request");
    } else if(q.size()!=4) return error("invalid_request");

    if(q["protocolVersion"]!=1) return error("unsupported_version",q["requestId"]);
    if(q["runId"]!=runId_) return error("run_mismatch",q["requestId"]);
    const auto id=q["requestId"].get<std::int64_t>();
    if(id<=lastId_) return error("duplicate_request_id",id);
    lastId_=id;
    if(input) {
        if(!ready_ || !playing_) return error("not_ready",id);
        if(inputState_==InputState::Queued || KeyboardIsolating()) return error("queue_full",id);
        inputState_=InputState::Queued;
        cancelInput_=false;
        keyboardReqId_=id;
        keyboardKeys_=std::move(keys);
        keyboardUpdatesRemaining_=updates;
        inputDeadline_=GetTickCount64()+5000;
        return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},{"status","queued"}};
    }
    if(mouse) {
        if(!ready_ || !playing_) return error("not_ready",id);
        if(mouseState_==InputState::Queued || MouseIsolating()) return error("queue_full",id);
        mouseState_=InputState::Queued;
        cancelMouse_=false;
        mouseReqId_=id;
        mouseDx_=mouseDx;
        mouseDy_=mouseDy;
        mouseButtons_=mouseButtonsMask;
        mouseWheel_=mouseWheel;
        mouseUpdatesRemaining_=mouseUpdates;
        mouseDeadline_=GetTickCount64()+5000;
        return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},{"status","queued"}};
    }
    if(controller) {
        if(!ready_ || !playing_) return error("not_ready",id);
        if(!controllerOp_.TryQueue(id, controllerCmd)) return error("queue_full",id);
        controllerDeadline_=GetTickCount64()+5000;
        return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},{"status","queued"}};
    }
    if(observe) {
        if(!ready_ || !playing_) return error("not_ready",id);
        for(const auto& t : targets_) {
            if(t.name == observeTarget) {
                return {
                    {"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},{"found",true},
                    {"target",t.name},
                    {"position",{{"x",t.posX},{"y",t.posY},{"z",t.posZ}}},
                    {"velocity",{{"x",t.velX},{"y",t.velY},{"z",t.velZ}}}
                };
            }
        }
        return {
            {"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},{"found",false},
            {"target",observeTarget}
        };
    }
    if(capture) {
        if(!ready_ || !playing_) return error("not_ready",id);
        if(captureState_.status == CaptureState::Status::Queued) return error("queue_full",id);
        captureState_.status = CaptureState::Status::Queued;
        captureState_.requestId = id;
        captureState_.width = 0;
        captureState_.height = 0;
        captureState_.basename.clear();
        captureState_.error.clear();
        return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},{"status","queued"}};
    }
    if(events) {
        if(!ready_ || !playing_) return error("not_ready",id);
        Json arr = Json::array();
        for (const auto& ev : events_) {
            arr.push_back({
                {"sequence", ev.sequence},
                {"type", ev.type},
                {"checkpoint", ev.checkpoint},
                {"actor", ev.actor},
                {"score", ev.score},
            });
        }
        return {
            {"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},
            {"events", std::move(arr)},
            {"latestSequence", eventsLatestSequence_},
            {"dropped", eventsDropped_},
        };
    }
    if(q["verb"]=="quit") {
        if(quitPending_ || quitReady_) return error("queue_full",id);
        quitPending_=true;
        return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},{"requiresReceipt",true}};
    }
    if(q["verb"]!="hello" && q["verb"]!="status") return error("unknown_verb",id);

    const char* capStatus = captureState_.status == CaptureState::Status::Queued ? "queued" :
                            captureState_.status == CaptureState::Status::Completed ? "completed" :
                            captureState_.status == CaptureState::Status::Failed ? "failed" : "idle";

    return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",true},
        {"pid",pid_},{"sessionId",session_},{"ready",ready_},{"playing",playing_},
        {"state",ready_ ? (playing_ ? "playing" : "ready") : "loading"},{"scene",scene_},
        {"capabilities",Json::array({"hello","status","quit","keyboard","observe","capture","mouse","controller","events"})},
        {"keyboard",{{"status",InputStatus()},{"requestId",keyboardReqId_},
            {"remainingUpdates",keyboardUpdatesRemaining_},{"cancelRequested",cancelInput_},{"preparing",inputState_==InputState::Preparing}}},
        {"mouse",{{"status",MouseStatus()},{"requestId",mouseReqId_},
            {"remainingUpdates",mouseUpdatesRemaining_},{"cancelRequested",cancelMouse_}}},
        {"controller",{{"status",ControllerStatus()},{"requestId",controllerOp_.RequestId()},
            {"remainingUpdates",controllerOp_.Remaining()},{"cancelRequested",controllerOp_.CancelRequested()}}},
        {"capture",{{"status",capStatus},{"requestId",captureState_.requestId},
            {"width",captureState_.width},{"height",captureState_.height},{"basename",captureState_.basename},{"error",captureState_.error}}}};
}
}

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sddl.h>
#include <mutex>
#include <thread>
#include <vector>
namespace ivy::testing {
struct Bridge::Impl {
    HANDLE stop = nullptr, pipe = INVALID_HANDLE_VALUE;
    std::string name;
    Protocol protocol;
    CaptureHandler captureHandler = nullptr;
    std::mutex mutex;
    std::thread worker;
    Impl(std::string run, DWORD pid, DWORD session) : protocol(run,pid,session) {
        name="\\\\.\\pipe\\IvyRuntimeTest-"+std::to_string(pid)+"-"+run;
    }
    ~Impl() {
        if(stop) SetEvent(stop);
        if(worker.joinable()) worker.join();
        if(pipe!=INVALID_HANDLE_VALUE) CloseHandle(pipe);
        if(stop) CloseHandle(stop);
    }
    bool Finish(BOOL immediate, OVERLAPPED& ov, DWORD& bytes, DWORD timeout) {
        if(immediate) return true;
        if(GetLastError()!=ERROR_IO_PENDING) return false;
        HANDLE waits[]{stop,ov.hEvent};
        const auto result=WaitForMultipleObjects(2,waits,FALSE,timeout);
        if(result!=WAIT_OBJECT_0+1) {
            CancelIoEx(pipe,&ov);
            GetOverlappedResult(pipe,&ov,&bytes,TRUE);
            return false;
        }
        return GetOverlappedResult(pipe,&ov,&bytes,FALSE)!=FALSE;
    }
    bool Byte(char& c, DWORD timeout) {
        OVERLAPPED ov{}; ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(!ov.hEvent) return false;
        DWORD n=0;
        bool ok=Finish(ReadFile(pipe,&c,1,&n,&ov),ov,n,timeout) && n==1;
        CloseHandle(ov.hEvent); return ok;
    }
    bool Send(const std::string& text) {
        OVERLAPPED ov{}; ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(!ov.hEvent) return false;
        DWORD n=0;
        bool ok=Finish(WriteFile(pipe,text.data(),static_cast<DWORD>(text.size()),&n,&ov),ov,n,2000)
            && n==text.size();
        CloseHandle(ov.hEvent); return ok;
    }
    void Run() {
        while(WaitForSingleObject(stop,0)==WAIT_TIMEOUT) {
            OVERLAPPED ov{}; ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
            if(!ov.hEvent) break;
            DWORD n=0;
            const BOOL connected=ConnectNamedPipe(pipe,&ov);
            const bool already=!connected && GetLastError()==ERROR_PIPE_CONNECTED;
            const bool ok=already || Finish(connected,ov,n,INFINITE);
            CloseHandle(ov.hEvent);
            if(!ok) break;
            bool receipt=false;
            for(;;) {
                std::string frame;
                char c=0;
                const ULONGLONG deadline=GetTickCount64()+2000;
                bool complete=false;
                for (;;) {
                    const auto timeout = RemainingFrameTimeout(deadline, GetTickCount64());
                    if (timeout == 0 || !Byte(c, timeout)) break;
                    if(c=='\n') {complete=true; break;}
                    frame+=c;
                    if(frame.size()>4096) {complete=true;break;}
                }
                if(!complete) break;
                Json response;
                { std::lock_guard lock(mutex); response=protocol.Request(frame); }
                if(!Send(response.dump(-1, ' ', false, Json::error_handler_t::replace)+"\n")) break;
                if(response.value("requiresReceipt",false)) {
                    if(Byte(c,2000) && c=='\n') {
                        std::lock_guard lock(mutex); protocol.ConfirmQuitReceipt(); receipt=true;
                    }
                    break;
                }
                if(frame.size()>4096) break;
            }
            { std::lock_guard lock(mutex); protocol.AbandonQuit(); protocol.CancelKeyboard(); protocol.CancelMouse(); protocol.CancelController(); }
            if(receipt) {
                WaitForSingleObject(stop,INFINITE); break;
            }
            DisconnectNamedPipe(pipe);
        }
    }
};
Bridge::Bridge() = default;
Bridge::~Bridge() { Stop(); }
bool Bridge::StartFromEnvironment() {
    if(impl_) return true;
    char enabled[4]{}, run[65]{};
    if(GetEnvironmentVariableA("IVY_RUNTIME_TEST_BRIDGE",enabled,4)!=1 || enabled[0]!='1') return false;
    const DWORD length=GetEnvironmentVariableA("IVY_RUNTIME_TEST_RUN_ID",run,65);
    if(length<1 || length>64) return false;
    for(DWORD i=0;i<length;++i) if(!((run[i]>='a'&&run[i]<='z') || (run[i]>='A'&&run[i]<='Z') || (run[i]>='0'&&run[i]<='9') || run[i]=='-')) return false;
    DWORD session=0;
    if(!ProcessIdToSessionId(GetCurrentProcessId(),&session)) return false;
    auto p=std::make_unique<Impl>(run,GetCurrentProcessId(),session);
    HANDLE token=nullptr;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) return false;
    DWORD size=0; GetTokenInformation(token,TokenUser,nullptr,0,&size);
    std::vector<unsigned char> buffer(size);
    const bool tokenOk=GetTokenInformation(token,TokenUser,buffer.data(),size,&size)!=FALSE;
    CloseHandle(token);
    if(!tokenOk) return false;
    LPSTR sid=nullptr;
    if(!ConvertSidToStringSidA(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid,&sid)) return false;
    const std::string dacl="D:P(A;;GA;;;"+std::string(sid)+")";
    LocalFree(sid);
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorA(dacl.c_str(),SDDL_REVISION_1,&descriptor,nullptr)) return false;
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),descriptor,FALSE};
    p->stop=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(p->stop) p->pipe=CreateNamedPipeA(p->name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,8192,8192,0,&security);
    LocalFree(descriptor);
    if(!p->stop || p->pipe==INVALID_HANDLE_VALUE) return false;
    try { p->worker=std::thread([ptr=p.get()] {
        try { ptr->Run(); }
        catch (const std::exception&) {
            OutputDebugStringA("RuntimeTestBridge: worker failed; disconnecting endpoint.\n");
            { std::lock_guard lock(ptr->mutex); ptr->protocol.CancelKeyboard(); ptr->protocol.CancelMouse(); ptr->protocol.CancelController(); }
            DisconnectNamedPipe(ptr->pipe);
            SetEvent(ptr->stop);
        }
    }); }
    catch(...) { return false; }
    RuntimeTestEvents_Reset();
    RuntimeTestEvents_SetEnabled(true);
    impl_=std::move(p); return true;
}
void Bridge::Stop() {
    if (impl_) {
        if (Keyboard_IsInjectionActive()) Keyboard_ClearInjection();
        if (Mouse_IsInjectionActive()) Mouse_ClearInjection();
        if (Controller_IsInjectionActive()) Controller_ClearInjection();
    }
    impl_.reset();
    RuntimeTestEvents_SetEnabled(false);
}
std::string Bridge::PipeName() const { return impl_ ? impl_->name : std::string{}; }

void Bridge::SetCaptureHandler(CaptureHandler handler) {
    if (impl_) impl_->captureHandler = handler;
}

static Keyboard_Keys ParseKeyName(const std::string& name) {
    if (name == "W") return KK_W;
    if (name == "A") return KK_A;
    if (name == "S") return KK_S;
    if (name == "D") return KK_D;
    if (name == "SPACE") return KK_SPACE;
    if (name == "UP") return KK_UP;
    if (name == "DOWN") return KK_DOWN;
    if (name == "LEFT") return KK_LEFT;
    if (name == "RIGHT") return KK_RIGHT;
    return KK_NONE;
}

void Bridge::PollAndApplyInput(bool inGamePlaying) {
    if(!impl_) return;
    std::lock_guard lock(impl_->mutex);
    impl_->protocol.BeginInput(inGamePlaying, GetTickCount64());
    if(impl_->protocol.KeyboardIsolating()) {
        std::vector<Keyboard_Keys> keys;
        for(const auto& name : impl_->protocol.InputKeys()) keys.push_back(ParseKeyName(name));
        Keyboard_TestInject(keys.data(), static_cast<int>(keys.size()));
    } else {
        Keyboard_ClearInjection();
    }
    if(impl_->protocol.MouseIsolating()) {
        int mdx = 0, mdy = 0, mwheel = 0;
        unsigned int mbuttons = 0;
        impl_->protocol.GetMouseInput(mdx, mdy, mbuttons, mwheel);
        Mouse_TestInject(mdx, mdy, mbuttons, mwheel);
    } else {
        Mouse_ClearInjection();
    }
    if(impl_->protocol.ControllerIsolating()) {
        float clx = 0.0f, cly = 0.0f, crx = 0.0f, cry = 0.0f, clt = 0.0f, crt = 0.0f;
        std::uint16_t cbuttons = 0;
        impl_->protocol.GetControllerInput(clx, cly, crx, cry, clt, crt, cbuttons);
        Controller_TestInjectSample(clx, cly, crx, cry, clt, crt, cbuttons);
    } else {
        Controller_ClearInjection();
    }
}
void Bridge::FinishInputUpdate() {
    if(!impl_) return;
    std::lock_guard lock(impl_->mutex);
    impl_->protocol.ExpireKeyboard(GetTickCount64());
    impl_->protocol.ExpireMouse(GetTickCount64());
    impl_->protocol.ExpireController(GetTickCount64());
    impl_->protocol.EndInput();
}
bool Bridge::PublishAndTakeQuit(bool ready, bool playing, const std::string& scene, std::vector<ObservedTarget> targets) {
    if(!impl_) return false;
    std::lock_guard lock(impl_->mutex);
    impl_->protocol.Publish(ready,playing,scene,std::move(targets));
    return impl_->protocol.TakeQuit();
}

void Bridge::MaybeExecuteCapture() {
    if(!impl_) return;
    std::int64_t reqId = 0;
    {
        std::lock_guard lock(impl_->mutex);
        if(!impl_->protocol.HasPendingCapture(reqId)) return;
    }
    if(!impl_->captureHandler) {
        std::lock_guard lock(impl_->mutex);
        impl_->protocol.CompleteCapture(reqId, false, 0, 0, "", "capture_unavailable");
        return;
    }
    char outDir[MAX_PATH]{};
    const DWORD len = GetEnvironmentVariableA("IVY_RUNTIME_TEST_OUTPUT_DIR", outDir, MAX_PATH);
    if(len == 0 || len >= MAX_PATH) {
        std::lock_guard lock(impl_->mutex);
        impl_->protocol.CompleteCapture(reqId, false, 0, 0, "", "output_dir_not_configured");
        return;
    }
    const std::string runId = impl_->protocol.RunId();
    const std::string basename = "capture-" + runId + "-" + std::to_string(reqId) + ".png";
    const std::string fullPath = std::string(outDir) + "\\" + basename;
    std::wstring wpath(fullPath.begin(), fullPath.end());
    uint32_t w = 0, h = 0;
    const bool ok = impl_->captureHandler(wpath.c_str(), &w, &h);
    {
        std::lock_guard lock(impl_->mutex);
        impl_->protocol.CompleteCapture(reqId, ok, w, h, ok ? basename : "", ok ? "" : "capture_failed");
    }
}
}

#endif
