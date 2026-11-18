// save_store_test.cpp
// Linux-runnable unit tests for engine/save/save_store.{h,cpp} (real files in a temp dir).
//
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/save_store_test tests/save_store_test.cpp engine/save/save_store.cpp
//   /tmp/save_store_test

#include "engine/save/save_store.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using save_store::Store;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static std::string ReadAll(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

static void WriteAll(const fs::path& p, const std::string& text) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << text;
}

int main() {
    const fs::path root = fs::temp_directory_path() / ("ivy_save_store_test_" + std::to_string(std::rand()));
    fs::remove_all(root);
    const fs::path dir = root / "saves";  // created by the first Commit
    std::vector<std::string> logs;
    auto log = [&](const std::string& l) { logs.push_back(l); };

    // --- names ---------------------------------------------------------------
    Expect(save_store::IsValidSlotName("slot1"), "slot1 valid");
    Expect(save_store::IsValidSlotName("Auto_Save-2"), "underscore / dash valid");
    Expect(save_store::IsValidSlotName(std::string(32, 'a')), "32 chars valid");
    Expect(!save_store::IsValidSlotName(std::string(33, 'a')), "33 chars invalid");
    Expect(!save_store::IsValidSlotName(""), "empty invalid");
    Expect(!save_store::IsValidSlotName("../x"), "traversal invalid");
    Expect(!save_store::IsValidSlotName("a.b"), "dot invalid");
    Expect(!save_store::IsValidSlotName("a b"), "space invalid");
    Expect(!save_store::IsValidSlotName("存檔"), "non-ASCII invalid");
    Expect(save_store::IsValidKey("player.hp"), "dotted key valid");
    Expect(save_store::IsValidKey("金幣"), "UTF-8 key valid");
    Expect(!save_store::IsValidKey(""), "empty key invalid");
    Expect(!save_store::IsValidKey("a\nb"), "control char key invalid");
    Expect(!save_store::IsValidKey(std::string(129, 'k')), "129-byte key invalid");

    Expect(save_store::SanitizeFolderName("My Game", "IvyGame") == "My Game", "plain name kept");
    Expect(save_store::SanitizeFolderName("a/b:c*?", "IvyGame") == "a_b_c__", "unsafe chars replaced");
    Expect(save_store::SanitizeFolderName("  .. ", "IvyGame") == "IvyGame", "blank -> fallback");
    Expect(save_store::SanitizeFolderName("", "IvyGame") == "IvyGame", "empty -> fallback");
    Expect(save_store::SanitizeFolderName("con", "IvyGame") == "_con", "reserved name escaped");
    Expect(save_store::SanitizeFolderName("COM1.txt", "IvyGame") == "_COM1.txt", "reserved with ext escaped");
    Expect(save_store::SanitizeFolderName("磁力場 2", "IvyGame") == "磁力場 2", "UTF-8 kept");
    Expect(save_store::SanitizeFolderName(" Game. ", "IvyGame") == "Game", "trim spaces and dots");

    // --- set / get / commit / reload ----------------------------------------
    {
        Store s(dir, save_store::DefaultReplaceFile, log);
        Expect(!s.SlotExists("slot1"), "no slot before commit");
        Expect(s.SetInt("slot1", "coins", 120), "set int");
        Expect(s.SetDouble("slot1", "time", 12.5), "set double");
        Expect(s.SetBool("slot1", "boss", true), "set bool");
        Expect(s.SetString("slot1", "level", "森林"), "set string");
        Expect(s.SetJsonText("slot1", "player", R"({"hp":80,"items":["key"]})"), "set json");
        Expect(!s.SetJsonText("slot1", "bad", "{oops"), "bad json rejected");
        Expect(!s.SetInt("../x", "k", 1), "bad slot rejected");
        Expect(!s.SetInt("slot1", "", 1), "bad key rejected");
        std::int64_t i = 0;
        Expect(s.GetInt("slot1", "coins", &i) && i == 120, "get int from memory");
        Expect(!s.SlotExists("slot1"), "uncommitted slot not on disk");
        Expect(s.Commit("slot1"), "commit");
        Expect(s.SlotExists("slot1"), "slot exists after commit");
        Expect(fs::exists(s.MainPath("slot1")) && !fs::exists(s.TempPath("slot1")), "main written, no temp left");
        Expect(!fs::exists(s.BackupPath("slot1")), "no backup on first commit");
        const std::string text = ReadAll(s.MainPath("slot1"));
        Expect(text.find("\"format\": \"ivy-save\"") != std::string::npos, "format field");
        Expect(text.find("\"savedAt\"") != std::string::npos, "savedAt field");
    }
    {
        Store s(dir, save_store::DefaultReplaceFile, log);  // fresh: reads disk
        std::int64_t i = 0;
        double d = 0;
        bool b = false;
        std::string str, js;
        Expect(s.GetInt("slot1", "coins", &i) && i == 120, "int survives reload");
        Expect(s.GetDouble("slot1", "time", &d) && d == 12.5, "double survives reload");
        Expect(s.GetDouble("slot1", "coins", &d) && d == 120.0, "int readable as double");
        Expect(!s.GetInt("slot1", "time", &i), "double not readable as int");
        Expect(s.GetBool("slot1", "boss", &b) && b, "bool survives reload");
        Expect(s.GetString("slot1", "level", &str) && str == "森林", "UTF-8 string survives reload");
        Expect(!s.GetString("slot1", "coins", &str), "wrong type -> false");
        Expect(s.GetJsonText("slot1", "player", &js) && js == R"({"hp":80,"items":["key"]})", "json survives reload");
        Expect(s.GetJsonText("slot1", "coins", &js) && js == "120", "json text of a number");
        Expect(s.HasKey("slot1", "boss") && !s.HasKey("slot1", "nope"), "has key");
        Expect(s.DeleteKey("slot1", "boss") && !s.HasKey("slot1", "boss"), "delete key");
        Expect(!s.DeleteKey("slot1", "boss"), "delete missing key -> false");

        // Second commit keeps the first file as backup.
        Expect(s.SetInt("slot1", "coins", 200) && s.Commit("slot1"), "second commit");
        Expect(fs::exists(s.BackupPath("slot1")), "backup kept");
        Expect(ReadAll(s.BackupPath("slot1")).find("120") != std::string::npos, "backup holds old value");
    }

    // --- corrupt main falls back to backup, and the backup is not overwritten
    {
        Store s(dir, save_store::DefaultReplaceFile, log);
        WriteAll(s.MainPath("slot1"), "{ truncated");
        logs.clear();
        std::int64_t i = 0;
        Expect(s.GetInt("slot1", "coins", &i) && i == 120, "corrupt main -> backup value");
        Expect(!logs.empty() && logs[0].find("loaded the backup") != std::string::npos, "fallback logged");
        Expect(s.SetInt("slot1", "coins", 130) && s.Commit("slot1"), "commit after fallback");
        Expect(ReadAll(s.BackupPath("slot1")).find("120") != std::string::npos,
               "good backup not replaced by the corrupt main");
        Store again(dir, save_store::DefaultReplaceFile, log);
        Expect(again.GetInt("slot1", "coins", &i) && i == 130, "new main readable");
    }

    // --- both unreadable -> empty slot, logged
    {
        Store s(dir, save_store::DefaultReplaceFile, log);
        WriteAll(s.MainPath("broken"), "nope");
        WriteAll(s.BackupPath("broken"), R"({"format":"other","version":1,"data":{}})");
        logs.clear();
        Expect(!s.HasKey("broken", "x"), "unreadable slot starts empty");
        Expect(!logs.empty() && logs[0].find("starting empty") != std::string::npos, "empty start logged");
    }

    // --- newer format is read-only
    {
        Store s(dir, save_store::DefaultReplaceFile, log);
        WriteAll(s.MainPath("future"), R"({"format":"ivy-save","version":99,"data":{"x":5}})");
        std::int64_t i = 0;
        Expect(s.GetInt("future", "x", &i) && i == 5, "newer format readable");
        Expect(!s.SetInt("future", "x", 6), "newer format not writable");
        Expect(!s.DeleteKey("future", "x"), "newer format no delete key");
        Expect(!s.Commit("future"), "newer format no commit");
        Expect(ReadAll(s.MainPath("future")).find("99") != std::string::npos, "newer file untouched");
    }

    // --- failing replace leaves the old file and no temp
    {
        auto failReplace = [](const fs::path&, const fs::path&, const fs::path&, std::string* e) {
            if (e) *e = "simulated failure";
            return false;
        };
        Store s(dir, failReplace, log);
        std::int64_t i = 0;
        Expect(s.GetInt("slot1", "coins", &i) && i == 130, "read before failing commit");
        Expect(s.SetInt("slot1", "coins", 999), "set before failing commit");
        logs.clear();
        Expect(!s.Commit("slot1"), "failing commit returns false");
        Expect(!fs::exists(s.TempPath("slot1")), "temp removed after failure");
        Expect(!logs.empty() && logs[0].find("simulated failure") != std::string::npos, "failure logged");
        Store fresh(dir, save_store::DefaultReplaceFile, log);
        Expect(fresh.GetInt("slot1", "coins", &i) && i == 130, "old value still on disk");
    }

    // --- size limit
    {
        Store s(dir, save_store::DefaultReplaceFile, log);
        Expect(s.SetString("big", "blob", std::string(save_store::kMaxFileBytes, 'x')), "set big string");
        Expect(!s.Commit("big"), "over 4 MiB rejected");
        Expect(!s.SlotExists("big"), "big slot not written");
    }

    // --- list, delete, drop cache
    {
        Store s(dir, save_store::DefaultReplaceFile, log);
        Expect(s.SetInt("auto", "n", 1) && s.Commit("auto"), "commit auto");
        WriteAll(dir / "notes.txt", "x");
        WriteAll(dir / "bad name.json", "{}");
        fs::remove(s.MainPath("future"));
        WriteAll(s.BackupPath("onlybak"), R"({"format":"ivy-save","version":1,"data":{}})");
        const auto names = s.ListSlots();
        const std::vector<std::string> want = {"auto", "broken", "onlybak", "slot1"};
        Expect(names == want, "list slots (sorted, valid names, .bak-only included)");
        Expect(s.SlotExists("onlybak"), "backup-only slot exists");
        Expect(s.DeleteSlot("slot1"), "delete slot");
        Expect(!s.SlotExists("slot1") && !fs::exists(s.BackupPath("slot1")), "slot files gone");
        std::int64_t i = 0;
        Expect(!s.GetInt("slot1", "coins", &i), "deleted slot empty in memory");
        Expect(!s.DeleteSlot("../x"), "delete invalid slot -> false");

        Expect(s.SetInt("auto", "n", 2), "uncommitted change");
        s.DropCache();
        Expect(s.GetInt("auto", "n", &i) && i == 1, "drop cache discards uncommitted change");
    }

    // --- empty directory that does not exist
    {
        Store s(root / "missing", save_store::DefaultReplaceFile, log);
        Expect(s.ListSlots().empty(), "missing dir -> no slots");
        Expect(!s.SlotExists("a"), "missing dir -> slot absent");
    }

    fs::remove_all(root);
    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("save_store_test: all checks passed\n");
    return 0;
}
