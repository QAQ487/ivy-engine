// editor_console_log_test.cpp
// Linux-runnable unit tests for console ring buffer / filter / collapse.
//
// Prefer: ./tools/run_editor_console_log_check.sh

#include "engine/editor/editor_console_log.h"

#include <cstdio>
#include <string>
#include <thread>
#include <vector>

static int g_failed = 0;

static void Expect(bool cond, const char* msg)
{
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static void TestInferLevel()
{
    Expect(InferConsoleLevel("hello") == EditorConsoleLevel::Info, "plain info");
    Expect(InferConsoleLevel("Warning: soft") == EditorConsoleLevel::Warning, "warning");
    Expect(InferConsoleLevel("WARN skip") == EditorConsoleLevel::Warning, "warn");
    Expect(InferConsoleLevel("Error: boom") == EditorConsoleLevel::Error, "error");
    Expect(InferConsoleLevel("FAIL: assert") == EditorConsoleLevel::Error, "fail");
    Expect(InferConsoleLevel("fatal crash") == EditorConsoleLevel::Error, "fatal");
}

static void TestFilter()
{
    EditorConsoleEntry info{EditorConsoleLevel::Info, 0, "alpha", 1};
    EditorConsoleEntry warn{EditorConsoleLevel::Warning, 0, "beta", 1};
    EditorConsoleEntry err{EditorConsoleLevel::Error, 0, "gamma", 1};

    EditorConsoleFilter f{};
    Expect(ConsoleEntryPassesFilter(info, f), "default shows info");
    f.showInfo = false;
    Expect(!ConsoleEntryPassesFilter(info, f), "hide info");
    Expect(ConsoleEntryPassesFilter(warn, f), "warn still shown");
    f.search = "amm";
    Expect(ConsoleEntryPassesFilter(err, f), "search gamma");
    Expect(!ConsoleEntryPassesFilter(warn, f), "search miss");
}

static void TestRingAndCollapse()
{
    EditorConsoleBuffer buf(4);
    buf.Push("a", EditorConsoleLevel::Info, 1);
    buf.Push("a", EditorConsoleLevel::Info, 2);
    buf.Push("a", EditorConsoleLevel::Info, 3);
    Expect(buf.Size() == 1, "collapse identical");
    auto snap = buf.SnapshotAll();
    Expect(snap.size() == 1 && snap[0].repeatCount == 3, "repeat count 3");

    buf.Push("b", EditorConsoleLevel::Warning, 4);
    buf.Push("c", EditorConsoleLevel::Error, 5);
    buf.Push("d", EditorConsoleLevel::Info, 6);
    buf.Push("e", EditorConsoleLevel::Info, 7); // drops oldest "a"
    Expect(buf.Size() == 4, "capacity 4");
    snap = buf.SnapshotAll();
    Expect(snap.front().text == "b", "oldest dropped");
}

static void TestSnapshotFilter()
{
    EditorConsoleBuffer buf;
    buf.Push("keep me", EditorConsoleLevel::Info, 1);
    buf.Push("noise", EditorConsoleLevel::Warning, 2);
    EditorConsoleFilter f{};
    f.showWarning = false;
    f.search = "keep";
    auto snap = buf.Snapshot(f);
    Expect(snap.size() == 1 && snap[0].text == "keep me", "filtered snapshot");
}

static void TestThreadSafePush()
{
    EditorConsoleBuffer buf(5000);
    auto worker = [&](int id) {
        for (int i = 0; i < 200; ++i) {
            char line[64];
            std::snprintf(line, sizeof(line), "t%d-%d", id, i);
            buf.PushAutoLevel(line, i);
        }
    };
    std::thread a(worker, 1);
    std::thread b(worker, 2);
    a.join();
    b.join();
    Expect(buf.Size() == 400, "all threaded pushes kept");
}

static void TestCjkSearch()
{
    EditorConsoleBuffer buf;
    // "專案" UTF-8
    const char kCjk[] = "\xE5\xB0\x88\xE6\xA1\x88 loaded";
    buf.Push(kCjk, EditorConsoleLevel::Info, 1);
    EditorConsoleFilter f{};
    f.search = "\xE5\xB0\x88\xE6\xA1\x88";
    Expect(buf.Snapshot(f).size() == 1, "CJK substring match");
}

int main()
{
    TestInferLevel();
    TestFilter();
    TestRingAndCollapse();
    TestSnapshotFilter();
    TestThreadSafePush();
    TestCjkSearch();

    if (g_failed != 0) {
        std::fprintf(stderr, "editor_console_log_test: %d failure(s)\n", g_failed);
        return 1;
    }
    std::printf("editor_console_log_test: PASS\n");
    return 0;
}
