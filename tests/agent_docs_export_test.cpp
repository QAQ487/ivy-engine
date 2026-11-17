// agent_docs_export_test.cpp
// Linux-runnable unit tests for engine/editor/agent_docs_export (File > New Project docs export).
//
// Build from the repo root (or run tools/run_agent_docs_export_check.sh, which also
// diffs the result against tools/export_agent_docs.py):
//   g++ -std=c++20 -Wall -Wextra -I. -o /tmp/agent_docs_export_test tests/agent_docs_export_test.cpp engine/editor/agent_docs_export.cpp
//   /tmp/agent_docs_export_test                    # unit tests
//   /tmp/agent_docs_export_test --export <dir>     # export the real engine docs into <dir>

#include "engine/editor/agent_docs_export.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static std::string Read(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static void Write(const fs::path& p, const std::string& s) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary | std::ios::trunc) << s;
}

static fs::path RepoRoot() {
    return fs::path(__FILE__).parent_path().parent_path();
}

static void TestMakeManagedBlock() {
    using namespace agent_docs;
    const std::string block = MakeManagedBlock("\r\n## Title\r\nline\r\n\r\n");
    Expect(block == std::string(kBeginMarker) + "\n## Title\nline\n" + kEndMarker + "\n",
           "block trims, normalizes CRLF, wraps in markers");
}

static void TestUpsert() {
    using namespace agent_docs;
    const std::string block = MakeManagedBlock("body v1");
    std::string out;

    Expect(UpsertManagedSection("", block, &out) && out == block, "empty file gets block only");

    Expect(UpsertManagedSection("# Game\nintro", block, &out) &&
               out == "# Game\nintro\n\n" + block,
           "append adds newline + blank line");

    const std::string first = out;
    const std::string block2 = MakeManagedBlock("body v2");
    Expect(UpsertManagedSection(first + "tail\n", block2, &out) &&
               out == "# Game\nintro\n\n" + block2 + "tail\n",
           "replace in place keeps outside content");

    std::string again;
    Expect(UpsertManagedSection(out, block2, &again) && again == out, "idempotent");

    std::string untouched = "keep";
    Expect(!UpsertManagedSection(std::string("x ") + kBeginMarker + " y", block, &untouched) &&
               untouched == "keep",
           "lone begin marker is rejected");
    Expect(!UpsertManagedSection(std::string(kEndMarker), block, &untouched), "lone end marker rejected");
}

static void TestRewrite() {
    Expect(agent_docs::RewriteExportedMarkdown("[s](../schemas/a.json) [t](../schemas/b.json)") ==
               "[s](schemas/a.json) [t](schemas/b.json)",
           "schema links rewritten");
    Expect(agent_docs::RewriteExportedMarkdown("[x](../other.md)") == "[x](../other.md)",
           "other links untouched");
}

static void TestReadEngineCommit(const fs::path& tmp) {
    const std::string hash = "0123456789abcdef0123456789abcdef01234567";
    const std::string hash2 = "89abcdef0123456789abcdef0123456789abcdef";

    const fs::path detached = tmp / "detached";
    Write(detached / ".git" / "HEAD", hash + "\n");
    Expect(agent_docs::ReadEngineCommit(detached) == hash, "detached HEAD");

    const fs::path loose = tmp / "loose";
    Write(loose / ".git" / "HEAD", "ref: refs/heads/main\n");
    Write(loose / ".git" / "refs" / "heads" / "main", hash + "\n");
    Expect(agent_docs::ReadEngineCommit(loose) == hash, "loose ref");

    const fs::path packed = tmp / "packed";
    Write(packed / ".git" / "HEAD", "ref: refs/heads/main\n");
    Write(packed / ".git" / "packed-refs",
          "# pack-refs with: peeled fully-peeled sorted\n" + hash2 + " refs/heads/other\n" + hash +
              " refs/heads/main\n");
    Expect(agent_docs::ReadEngineCommit(packed) == hash, "packed-refs");

    const fs::path worktree = tmp / "worktree";
    Write(worktree / ".git", "gitdir: ../wt-gitdir\n");
    Write(tmp / "wt-gitdir" / "HEAD", "ref: refs/heads/feature\n");
    Write(tmp / "wt-gitdir" / "commondir", "../common\n");
    Write(tmp / "common" / "refs" / "heads" / "feature", hash2 + "\n");
    Expect(agent_docs::ReadEngineCommit(worktree) == hash2, "worktree .git file + commondir");

    Expect(agent_docs::ReadEngineCommit(tmp / "nope") == "unknown", "missing .git -> unknown");
}

static void TestExport(const fs::path& tmp) {
    const fs::path engine = tmp / "engine";
    Write(engine / "docs" / "agent" / "README.md", "See [schema](../schemas/s.json).\n");
    Write(engine / "docs" / "agent" / "api" / "ui.md", "# UI\n");
    Write(engine / "docs" / "schemas" / "s.json", "{}\n");
    Write(engine / "tools" / "agent_docs_managed_section.md", "## Ivy Engine (exported docs)\nrules\n");
    Write(engine / ".git" / "HEAD", "0123456789abcdef0123456789abcdef01234567\n");

    const fs::path project = tmp / "project";
    Write(project / "AGENTS.md", "# My game\n");
    std::string error;
    Expect(agent_docs::ExportToContentProject(engine, project, &error), "export succeeds");
    const fs::path dest = project / "docs" / "ivy-engine";
    Expect(Read(dest / "README.md") == "See [schema](schemas/s.json).\n", "README copied + rewritten");
    Expect(fs::is_regular_file(dest / "api" / "ui.md"), "nested file copied");
    Expect(fs::is_regular_file(dest / "schemas" / "s.json"), "schemas copied");
    Expect(Read(dest / "ENGINE_COMMIT") == "0123456789abcdef0123456789abcdef01234567\n", "ENGINE_COMMIT");
    const std::string agents = Read(project / "AGENTS.md");
    Expect(agents.rfind("# My game\n\n", 0) == 0, "existing AGENTS.md content kept");
    Expect(agents.find("rules") != std::string::npos, "managed section appended");
    Expect(Read(project / "CLAUDE.md").rfind(agent_docs::kBeginMarker, 0) == 0, "CLAUDE.md created");

    Write(project / "CLAUDE.md", std::string("broken ") + agent_docs::kBeginMarker + "\n");
    Expect(!agent_docs::ExportToContentProject(engine, project, &error) &&
               error.find("Broken managed markers") != std::string::npos,
           "broken markers reported");

    fs::remove(engine / "tools" / "agent_docs_managed_section.md");
    Expect(!agent_docs::ExportToContentProject(engine, tmp / "p2", &error), "missing section text fails");
}

static void TestRealSectionText() {
    const std::string body = Read(RepoRoot() / agent_docs::kManagedSectionRel);
    Expect(body.find("### Engine boundary") != std::string::npos, "shared text has boundary rules");
    Expect(body.find("docs/engine-requests.md") != std::string::npos, "shared text names request log");
}

int main(int argc, char** argv) {
    if (argc == 3 && std::strcmp(argv[1], "--export") == 0) {
        std::string error;
        if (!agent_docs::ExportToContentProject(RepoRoot(), argv[2], &error)) {
            std::fprintf(stderr, "export failed: %s\n", error.c_str());
            return 1;
        }
        return 0;
    }

    const fs::path tmp = fs::temp_directory_path() / "agent_docs_export_test";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    TestMakeManagedBlock();
    TestUpsert();
    TestRewrite();
    TestReadEngineCommit(tmp / "git");
    TestExport(tmp / "export");
    TestRealSectionText();

    fs::remove_all(tmp);
    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("agent_docs_export_test: all checks passed\n");
    return 0;
}
