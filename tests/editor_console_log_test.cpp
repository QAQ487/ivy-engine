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
    // Tightened heuristics: do not treat substring hits inside other words.
    Expect(InferConsoleLevel("0 errors fixed") == EditorConsoleLevel::Info,
           "0 errors → info (plural not keyword)");
    Expect(InferConsoleLevel("failover path") == EditorConsoleLevel::Info,
           "failover → info");
    Expect(InferConsoleLevel("failed to load") == EditorConsoleLevel::Error,
           "failed → error");
}

static void TestFilter()
{
    EditorConsoleEntry info{1, EditorConsoleLevel::Info, 0, "alpha", 1};
    EditorConsoleEntry warn{2, EditorConsoleLevel::Warning, 0, "beta", 1};
    EditorConsoleEntry err{3, EditorConsoleLevel::Error, 0, "gamma", 1};

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

static void TestMultilineSplitAndCap()
{
    auto lines = SplitConsoleLogLines("one\ntwo\r\n\nthree");
    Expect(lines.size() == 3, "split 3 non-empty lines");
    Expect(lines[0] == "one" && lines[1] == "two" && lines[2] == "three", "split text");

    std::string huge(kEditorConsoleEntryMaxBytes + 100, 'x');
    lines = SplitConsoleLogLines(huge);
    Expect(lines.size() == 1, "single capped line");
    Expect(lines[0].size() == kEditorConsoleEntryMaxBytes, "cap 4KB");

    EditorConsoleBuffer buf;
    buf.Push("l1\nl2\nl3", EditorConsoleLevel::Info, 1);
    Expect(buf.Size() == 3, "push splits lines");
}

static void TestUtf8CapDoesNotSplitCjk()
{
    // "專" is E5 B0 88 (3 bytes). Fill so maxBytes lands mid-character.
    const char kChar[] = "\xE5\xB0\x88"; // 專
    std::string text;
    while (text.size() + 3 < 20)
        text.append(kChar);
    text.append(kChar); // ensure we exceed
    CapConsoleEntryText(text, 20);
    Expect(text.size() % 3 == 0, "CJK cap on char boundary");
    Expect(text.size() <= 20, "CJK cap <= max");
    // Last three bytes must form a complete character (not a continuation alone).
    if (!text.empty()) {
        const unsigned char lead = static_cast<unsigned char>(text[text.size() - 3]);
        Expect((lead & 0xF0u) == 0xE0u, "last CJK char intact");
    }
}

static void TestStableIdsAndSnapshotCache()
{
    EditorConsoleBuffer buf(3);
    buf.Push("a", EditorConsoleLevel::Info, 1);
    buf.Push("b", EditorConsoleLevel::Info, 2);
    auto all = buf.SnapshotAll();
    Expect(all.size() == 2, "two entries");
    const std::uint64_t idB = all[1].id;
    Expect(idB > all[0].id, "ids increase");

    EditorConsoleFilter f{};
    auto snap1 = buf.Snapshot(f);
    const std::uint64_t gen1 = buf.Generation();
    auto snap2 = buf.Snapshot(f);
    Expect(buf.Generation() == gen1, "cache does not bump generation");
    Expect(snap1 && snap2 && snap1.get() == snap2.get(), "shared_ptr cache hit");
    Expect(snap1->size() == 2 && (*snap1)[0].id == all[0].id, "cache contents");

    buf.Push("c", EditorConsoleLevel::Info, 3);
    buf.Push("d", EditorConsoleLevel::Info, 4); // drops a
    all = buf.SnapshotAll();
    Expect(all.size() == 3 && all.front().text == "b", "ring drop front");
    bool foundB = false;
    for (const auto& e : all) {
        if (e.id == idB) foundB = true;
    }
    Expect(foundB, "selection id survives ring drop");
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
    Expect(snap && snap->size() == 1 && (*snap)[0].text == "keep me", "filtered snapshot");
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
    const char kCjk[] = "\xE5\xB0\x88\xE6\xA1\x88 loaded";
    buf.Push(kCjk, EditorConsoleLevel::Info, 1);
    EditorConsoleFilter f{};
    f.search = "\xE5\xB0\x88\xE6\xA1\x88";
    Expect(buf.Snapshot(f) && buf.Snapshot(f)->size() == 1, "CJK substring match");
}

int main()
{
    TestInferLevel();
    TestFilter();
    TestRingAndCollapse();
    TestMultilineSplitAndCap();
    TestUtf8CapDoesNotSplitCjk();
    TestStableIdsAndSnapshotCache();
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
