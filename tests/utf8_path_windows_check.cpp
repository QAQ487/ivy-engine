// utf8_path_windows_check.cpp
// Windows-only ACP / CJK regression for content path helpers.
// Linux builds compile this file but skip the body (exit 0).
//
// Windows (MSVC, from repo root after vcvars64):
//   cl /nologo /EHsc /std:c++20 /utf-8 /DNOMINMAX /I. ^
//     tests\utf8_path_windows_check.cpp ^
//     /Fo:tools\phase6-test-build\ /Fe:tools\phase6-test-build\utf8_path_windows_check.exe
//   tools\phase6-test-build\utf8_path_windows_check.exe
// Or: tools\run_content_reload_checks.cmd (includes this check).

#include "engine/core/utf8_path.h"
#include "engine/core/content_path.h"
#include "engine/editor/content_file_monitor.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace fs = std::filesystem;

// UTF-8: "hp_fill - 複製.png" and "測試.png"
static const char kCopyPng[] = "hp_fill - \xE8\xA4\x87\xE8\xA3\xBD.png";
static const char kTestPng[] = "\xE6\xB8\xAC\xE8\xA9\xA6.png";

#ifndef _WIN32
int main() {
    std::printf("SKIP: utf8_path_windows_check (Windows-only ACP regression)\n");
    return 0;
}
#else

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static void TestRoundTripAndScan() {
    const fs::path root = fs::temp_directory_path() /
        ("ivy_utf8_win_" + std::to_string(GetTickCount64()));
    fs::remove_all(root);
    fs::create_directories(root / "texture");
    fs::create_directories(root / "rom" / "texture");

    const std::string copyLeaf(kCopyPng);
    const std::string testLeaf(kTestPng);
    const std::string copyRel = "texture/" + copyLeaf;
    const std::string testRel = "texture/" + testLeaf;
    const std::string romCopy = "rom/texture/" + copyLeaf;

    Expect(PathToUtf8(PathFromUtf8(copyLeaf)) == copyLeaf, "round-trip copy leaf");
    Expect(PathToUtf8(PathFromUtf8(testLeaf)) == testLeaf, "round-trip test leaf");
    Expect(PathToUtf8(PathFromUtf8(copyRel).filename()) == copyLeaf, "filename leaf");
    Expect(PathToUtf8(PathFromUtf8(copyLeaf).extension()) == ".png", "extension");
    Expect(PathToUtf8(PathFromUtf8(copyLeaf).stem()) ==
               std::string("hp_fill - \xE8\xA4\x87\xE8\xA3\xBD"),
           "stem");

    { std::ofstream(root / PathFromUtf8(copyRel), std::ios::binary) << "copy"; }
    { std::ofstream(root / PathFromUtf8(testRel), std::ios::binary) << "test"; }
    { std::ofstream(root / "rom" / "texture" / PathFromUtf8(copyLeaf), std::ios::binary) << "rom"; }

    const auto snap = ContentFileMonitor::Scan(root);
    Expect(snap.count(copyRel) == 1, "monitor scan copy name");
    Expect(snap.count(testRel) == 1, "monitor scan test name");

    // Asset-index style stringification of directory entries (no ACP .string()).
    std::error_code ec;
    bool sawCopy = false;
    for (fs::directory_iterator it(root / "texture", ec), end; it != end; ++it) {
        const std::string name = PathToUtf8(it->path().filename());
        const std::string rel = PathToUtf8(content_path::LexicalChildPath(it->path(), root));
        if (name == copyLeaf) {
            sawCopy = true;
            Expect(rel == copyRel, "LexicalChildPath UTF-8 relative");
        }
    }
    Expect(sawCopy, "directory walk saw copy leaf via PathToUtf8");

    const content_path::RomRoots roots{root, root / "missing-resources"};
    const fs::path resolved = content_path::ResolveRomForRead(romCopy, roots);
    Expect(!resolved.empty(), "ResolveRomForRead finds CJK file");
    Expect(PathToUtf8(resolved.filename()) == copyLeaf, "resolved filename UTF-8");

    fs::remove_all(root);
}

static void TestNarrowCtorMisdecodesUnderAcp() {
    const UINT acp = GetACP();
    std::fprintf(stdout, "ACP=%u\n", acp);

    const std::string utf8Leaf(kCopyPng);
    const fs::path correct = PathFromUtf8(utf8Leaf);

    // Old crash pattern: treat UTF-8 bytes as ACP when building a path.
    std::error_code ec;
    fs::path naive;
    bool naiveThrew = false;
    try {
        naive = fs::path(utf8Leaf);
    } catch (const std::system_error&) {
        naiveThrew = true;
    }

    if (acp == CP_UTF8) {
        // UTF-8 ACP: narrow ctor may round-trip; helpers must still match.
        Expect(!naiveThrew, "UTF-8 ACP: narrow ctor should not throw");
        Expect(PathToUtf8(naive) == utf8Leaf, "UTF-8 ACP: naive equals PathFromUtf8");
        Expect(PathToUtf8(correct) == utf8Leaf, "UTF-8 ACP: PathFromUtf8 round-trip");
        return;
    }

    // Non-UTF-8 ACP (e.g. 932): UTF-8 bytes are mis-decoded as ACP code units.
    // That yields a different wide path and/or throws — the editor crash class.
    if (naiveThrew) {
        Expect(true, "narrow path(std::string utf8) threw under non-UTF-8 ACP");
    } else {
        Expect(naive.native() != correct.native(),
               "narrow path(std::string utf8) mis-decodes under non-UTF-8 ACP");
    }

    // absolute(std::string) uses the same narrow conversion.
    bool absThrew = false;
    fs::path absNaive;
    try {
        absNaive = fs::absolute(utf8Leaf, ec);
        if (ec)
            absThrew = true;
    } catch (const std::system_error&) {
        absThrew = true;
    }
    fs::path absCorrect;
    std::error_code okEc;
    absCorrect = fs::absolute(PathFromUtf8(utf8Leaf), okEc);
    Expect(!okEc, "absolute(PathFromUtf8) succeeds");
    if (absThrew) {
        Expect(true, "absolute(std::string utf8) failed/threw under non-UTF-8 ACP");
    } else {
        Expect(absNaive.native() != absCorrect.native(),
               "absolute(std::string utf8) mis-decodes under non-UTF-8 ACP");
    }
}

int main() {
    try {
        TestRoundTripAndScan();
        TestNarrowCtorMisdecodesUnderAcp();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: unexpected exception: %s\n", e.what());
        return 1;
    }
    if (g_failed) {
        std::fprintf(stderr, "%d assertion(s) failed\n", g_failed);
        return 1;
    }
    std::printf("UTF8_PATH_WINDOWS_CHECK PASS\n");
    return 0;
}
#endif
