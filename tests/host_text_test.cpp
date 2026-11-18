// host_text_test.cpp
// Linux-runnable unit tests for engine/scripting/host_text.h.
//
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/host_text_test tests/host_text_test.cpp
//   /tmp/host_text_test

#include "engine/scripting/host_text.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

int main() {
    using namespace host_text;

    // CopyOut: length query, exact fit, truncation, empty.
    {
        Expect(CopyOut("hello", nullptr, 0) == 5, "length query");
        char buf[6];
        std::memset(buf, 'x', sizeof(buf));
        Expect(CopyOut("hello", buf, 6) == 5 && std::strcmp(buf, "hello") == 0, "exact fit");
        char small[3];
        Expect(CopyOut("hello", small, 3) == 5 && std::strcmp(small, "he") == 0, "truncated, NUL kept");
        char one[1] = {'x'};
        Expect(CopyOut("hello", one, 1) == 5 && one[0] == '\0', "capacity 1 -> empty string");
        char untouched[2] = {'x', 'y'};
        Expect(CopyOut("hello", untouched, 0) == 5 && untouched[0] == 'x', "capacity 0 writes nothing");
        Expect(CopyOut("hello", untouched, -4) == 5 && untouched[0] == 'x', "negative capacity writes nothing");
        char e[4];
        Expect(CopyOut("", e, 4) == 0 && e[0] == '\0', "empty text");
        const std::string utf8 = "森林";  // 6 bytes
        char u[16];
        Expect(CopyOut(utf8, u, 16) == 6 && utf8 == u, "UTF-8 bytes copied");
    }

    // ReadText: two passes through a host-style getter.
    {
        const std::string value = "{\"hp\":80}";
        int calls = 0;
        auto getter = [&](char* out, int cap) { ++calls; return CopyOut(value, out, cap); };
        std::string got;
        Expect(ReadText(getter, &got) && got == value && calls == 2, "two-pass read");

        auto missing = [](char*, int) { return -1; };
        got = "keep";
        Expect(!ReadText(missing, &got) && got == "keep", "missing -> false, out untouched");

        auto empty = [](char* out, int cap) { return CopyOut("", out, cap); };
        Expect(ReadText(empty, &got) && got.empty(), "empty value");

        // Value shrinks between the two calls: result is the shorter one.
        int n = 0;
        auto shrinking = [&](char* out, int cap) { return CopyOut(n++ == 0 ? "abcdef" : "ab", out, cap); };
        Expect(ReadText(shrinking, &got) && got == "ab", "shrinking value");

        Expect(!ReadText(getter, static_cast<std::string*>(nullptr)), "null out -> false");
    }

    // JoinLines / SplitLines round trip.
    {
        const std::vector<std::string> names = {"auto", "slot1", "slot_2"};
        Expect(JoinLines(names) == "auto\nslot1\nslot_2", "join");
        Expect(SplitLines(JoinLines(names)) == names, "round trip");
        Expect(SplitLines("").empty() && JoinLines({}).empty(), "empty list");
        Expect(SplitLines("one") == std::vector<std::string>{"one"}, "single item");
    }

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("host_text_test: all checks passed\n");
    return 0;
}
