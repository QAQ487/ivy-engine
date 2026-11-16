// utf8_path_test.cpp
// Linux-runnable checks for UTF-8 path helpers and content scan with CJK names.
//
// Build from the repo root:
//   g++ -std=c++20 -Wall -Wextra -I. -o /tmp/utf8_path_test tests/utf8_path_test.cpp
//   /tmp/utf8_path_test

#include "engine/core/utf8_path.h"
#include "engine/core/content_path.h"
#include "engine/editor/content_file_monitor.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

// UTF-8: "hp_fill - 複製.png" and "測試.png" (Explorer copy suffix / short name).
static const char kCopyPng[] = "hp_fill - \xE8\xA4\x87\xE8\xA3\xBD.png";
static const char kTestPng[] = "\xE6\xB8\xAC\xE8\xA9\xA6.png";

static void TestRoundTrip() {
    const std::string copyName(kCopyPng);
    const std::string testName(kTestPng);
    Expect(PathToUtf8(PathFromUtf8(copyName)) == copyName, "round-trip copy suffix");
    Expect(PathToUtf8(PathFromUtf8(testName)) == testName, "round-trip test name");
    Expect(PathToUtf8(PathFromUtf8(std::string("rom/texture/") + copyName)) ==
               std::string("rom/texture/") + copyName,
           "round-trip rom-relative copy path");
    Expect(PathToUtf8(PathFromUtf8(copyName).filename()) == copyName, "filename leaf");
    Expect(PathToUtf8(PathFromUtf8(copyName).extension()) == ".png", "ascii extension via utf8");
}

static void TestSafeRomAcceptsCjk() {
    const std::string romCopy = std::string("rom/texture/") + kCopyPng;
    const std::string romTest = std::string("rom/texture/") + kTestPng;
    Expect(content_path::IsSafeRomPath(romCopy, nullptr), "safe rom copy name");
    Expect(content_path::IsSafeRomPath(romTest, nullptr), "safe rom test name");
}

static void TestResolveRomReadCjk() {
    const fs::path scratch = fs::temp_directory_path() / "ivy_utf8_path_resolve";
    fs::remove_all(scratch);
    const fs::path project = scratch / "Game";
    const fs::path resources = scratch / "resources";
    fs::create_directories(project / "rom" / "texture");
    fs::create_directories(resources / "texture");
    const std::string leaf(kCopyPng);
    { std::ofstream(project / "rom" / "texture" / PathFromUtf8(leaf)) << "project"; }
    const content_path::RomRoots roots{project, resources};
    const std::string rom = std::string("rom/texture/") + leaf;
    const fs::path resolved = content_path::ResolveRomForRead(rom, roots);
    Expect(!resolved.empty(), "resolve finds CJK project file");
    Expect(PathToUtf8(resolved.filename()) == leaf, "resolved filename stays UTF-8");
    fs::remove_all(scratch);
}

static void TestContentMonitorScanCjk() {
    const fs::path root = fs::temp_directory_path() /
        ("ivy_utf8_scan_" + std::to_string(fs::hash_value(fs::temp_directory_path())));
    fs::remove_all(root);
    fs::create_directories(root / "texture");
    const std::string copyRel = std::string("texture/") + kCopyPng;
    const std::string testRel = std::string("texture/") + kTestPng;
    { std::ofstream(root / PathFromUtf8(copyRel)) << "copy"; }
    { std::ofstream(root / PathFromUtf8(testRel)) << "test"; }
    const auto snap = ContentFileMonitor::Scan(root);
    Expect(snap.count(copyRel) == 1, "scan finds Explorer copy suffix name");
    Expect(snap.count(testRel) == 1, "scan finds short CJK name");
    // Monitor keys must stay UTF-8 (generic_u8string), never ACP .string().
    Expect(PathToUtf8(PathFromUtf8(copyRel)) == copyRel, "scan key round-trips");
    fs::remove_all(root);
}

int main() {
    TestRoundTrip();
    TestSafeRomAcceptsCjk();
    TestResolveRomReadCjk();
    TestContentMonitorScanCjk();
    if (g_failed) {
        std::fprintf(stderr, "%d assertion(s) failed\n", g_failed);
        return 1;
    }
    std::printf("UTF8_PATH_TEST PASS\n");
    return 0;
}
