#include "runtime_test_bridge.h"
#if defined(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE)
namespace ivy::testing {
std::uint32_t RemainingFrameTimeout(std::uint64_t deadline, std::uint64_t now) noexcept {
    if (now >= deadline) return 0;
    const auto remaining = deadline - now;
    return static_cast<std::uint32_t>(remaining > 2000 ? 2000 : remaining);
}
Protocol::Protocol(std::string runId, unsigned long pid, unsigned long session)
    : runId_(std::move(runId)), pid_(pid), session_(session) {}
void Protocol::Publish(bool ready, bool playing, const std::string& scene) {
    ready_ = ready; playing_ = playing;
    std::size_t end = scene.size() < 1024 ? scene.size() : 1024;
    if (end < scene.size()) {
        while (end > 0 && (static_cast<unsigned char>(scene[end]) & 0xC0) == 0x80) --end;
    }
    scene_ = scene.substr(0, end);
}
bool Protocol::TakeQuit() { const bool result=quitReady_; quitReady_=false; return result; }
void Protocol::ConfirmQuitReceipt() { if(quitPending_) { quitReady_=true; quitPending_=false; } }
void Protocol::AbandonQuit() { quitPending_=false; }
Json Protocol::Request(const std::string& frame) {
    if(frame.size()>4096) return {{"protocolVersion",1},{"requestId",nullptr},{"runId",runId_},{"ok",false},{"error","message_too_large"}};
    auto q = Json::parse(frame, nullptr, false);
    if(q.is_discarded()) return {{"protocolVersion",1},{"requestId",nullptr},{"runId",runId_},{"ok",false},{"error","malformed_json"}};
    if (!q.is_object() || q.size()!=4 || !q.contains("protocolVersion") || !q["protocolVersion"].is_number_integer()
        || !q.contains("requestId") || !q["requestId"].is_number_integer() || q["requestId"] < 1 || q["requestId"] > 2147483647
        || !q.contains("runId") || !q["runId"].is_string() || !q.contains("verb") || !q["verb"].is_string())
        return {{"protocolVersion",1},{"requestId",nullptr},{"runId",runId_},{"ok",false},{"error","invalid_request"}};
    if (q["protocolVersion"] != 1) return {{"protocolVersion",1},{"requestId",q["requestId"]},{"runId",runId_},{"ok",false},{"error","unsupported_version"}};
    if(q["runId"] != runId_) return {{"protocolVersion",1},{"requestId",q["requestId"]},{"runId",runId_},{"ok",false},{"error","run_mismatch"}};
    const auto id = q["requestId"].get<std::int64_t>();
    if(id <= lastId_) return {{"protocolVersion",1},{"requestId",id},{"runId",runId_},{"ok",false},{"error","duplicate_request_id"}};
    lastId_ = id;
    if(q["verb"] != "hello" && q["verb"] != "status" && q["verb"] != "quit")
        return {{"protocolVersion",1},{"requestId",q["requestId"]},{"runId",runId_},{"ok",false},{"error","unknown_verb"}};
    if(q["verb"] == "quit") {
        if(quitPending_ || quitReady_) return {{"protocolVersion",1},{"requestId",q["requestId"]},{"runId",runId_},{"ok",false},{"error","queue_full"}};
        quitPending_=true;
        return {{"protocolVersion",1},{"requestId",q["requestId"]},{"runId",runId_},{"ok",true},{"requiresReceipt",true}};
    }
    return {{"protocolVersion",1},{"requestId",q["requestId"]},{"runId",runId_},
        {"ok",true},{"pid",pid_},{"sessionId",session_},{"ready",ready_},{"playing",playing_},{"state",ready_ ? (playing_ ? "playing" : "ready") : "loading"},{"scene",scene_},{"capabilities",Json::array({"hello","status","quit"})}};
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
    // Every pending operation is cancelled and reaped before its OVERLAPPED dies.
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
                    // A distinct transport receipt proves the client read the quit reply.
                    // Main-thread exit is never armed by a write into an unread pipe buffer.
                    if(Byte(c,2000) && c=='\n') {
                        std::lock_guard lock(mutex); protocol.ConfirmQuitReceipt(); receipt=true;
                    }
                    break;
                }
                if(frame.size()>4096) break;
            }
            { std::lock_guard lock(mutex); protocol.AbandonQuit(); }
            if(receipt) {
                // Keep the connection alive until main-thread normal shutdown cancels us.
                WaitForSingleObject(stop,INFINITE); break;
            }
            DisconnectNamedPipe(pipe);
        }
    }
};
Bridge::Bridge() = default;
Bridge::~Bridge() = default;
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
            DisconnectNamedPipe(ptr->pipe);
            SetEvent(ptr->stop);
        }
    }); }
    catch(...) { return false; }
    impl_=std::move(p); return true;
}
void Bridge::Stop() { impl_.reset(); }
std::string Bridge::PipeName() const { return impl_ ? impl_->name : std::string{}; }
bool Bridge::PublishAndTakeQuit(bool ready, bool playing, const std::string& scene) {
    if(!impl_) return false;
    std::lock_guard lock(impl_->mutex);
    impl_->protocol.Publish(ready,playing,scene);
    return impl_->protocol.TakeQuit();
}
}

#endif
