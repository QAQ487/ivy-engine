// debug_ofstream_tls_test.cpp
// Linux check: thread_local dout buffer flushes at thread/process exit without
// use-after-free when the global dout streambuf is destroyed later.
//
// Prefer: ./tools/run_debug_ofstream_tls_check.sh
//
// Modes:
//   (default) run worker-exit + local buf destruction checks
//   --exit-unflushed <path>  write hook writes to path, leave unflushed line, exit

#include "engine/debug_draw/debug_ofstream.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
std::mutex g_mu;
std::vector<std::string> g_captured;
std::atomic<int> g_hookCalls{0};
std::string g_fileSink;

void CaptureHook(const char* text, void*)
{
    if (!text)
        return;
    ++g_hookCalls;
    std::lock_guard<std::mutex> lock(g_mu);
    g_captured.emplace_back(text);
    if (!g_fileSink.empty()) {
        std::ofstream out(g_fileSink, std::ios::app);
        out << text;
    }
}

bool Contains(const std::string& needle)
{
    std::lock_guard<std::mutex> lock(g_mu);
    for (const auto& s : g_captured) {
        if (s.find(needle) != std::string::npos)
            return true;
    }
    return false;
}
} // namespace

static int RunExitUnflushedChild(const char* path)
{
    g_fileSink = path ? path : "";
    // Truncate sink.
    if (path) {
        std::ofstream trunc(path, std::ios::trunc);
    }
    hal::SetDebugLogHook(&CaptureHook, nullptr);
    // No endl/flush — TLS destructor at process exit must emit this.
    hal::dout << "main-exit-unflushed";
    return 0;
}

static int RunParentChecks(const char* selfPath)
{
    int failed = 0;
    g_fileSink.clear();
    g_captured.clear();
    g_hookCalls = 0;
    hal::SetDebugLogHook(&CaptureHook, nullptr);

    // Worker writes without endl/flush; ~DebugTlsBuffer at thread exit must flush.
    std::thread worker([] {
        hal::dout << "worker-unflushed-line";
    });
    worker.join();
    if (!Contains("worker-unflushed-line")) {
        std::fprintf(stderr, "FAIL: worker TLS exit did not flush\n");
        ++failed;
    }

    // Destroy a temporary streambuf while TLS is still alive — must not UAF (ASan).
    {
        hal::debugbuf localBuf;
    }

    hal::DebugWriteA("direct-write\n");
    if (!Contains("direct-write")) {
        std::fprintf(stderr, "FAIL: DebugWriteA not hooked\n");
        ++failed;
    }

#if !defined(_WIN32)
    // Child process: main-thread unflushed text flushed by TLS dtor at exit.
    const char* sink = "/tmp/debug_ofstream_tls_exit.txt";
    pid_t pid = fork();
    if (pid < 0) {
        std::fprintf(stderr, "FAIL: fork\n");
        ++failed;
    } else if (pid == 0) {
        execl(selfPath, selfPath, "--exit-unflushed", sink, static_cast<char*>(nullptr));
        _exit(127);
    } else {
        int status = 0;
        waitpid(pid, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            std::fprintf(stderr, "FAIL: child exit status %d\n", status);
            ++failed;
        } else {
            std::ifstream in(sink);
            std::string body((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
            if (body.find("main-exit-unflushed") == std::string::npos) {
                std::fprintf(stderr, "FAIL: main-thread exit did not flush TLS (%s)\n",
                             body.c_str());
                ++failed;
            }
        }
    }
#else
    (void)selfPath;
#endif

    hal::ClearDebugLogHook();
    if (failed != 0) {
        std::fprintf(stderr, "debug_ofstream_tls_test: %d failure(s)\n", failed);
        return 1;
    }
    std::printf("debug_ofstream_tls_test: PASS (hook_calls=%d)\n", g_hookCalls.load());
    return 0;
}

int main(int argc, char** argv)
{
    if (argc >= 3 && std::strcmp(argv[1], "--exit-unflushed") == 0)
        return RunExitUnflushedChild(argv[2]);
    return RunParentChecks(argv[0]);
}
