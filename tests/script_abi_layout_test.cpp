// script_abi_layout_test.cpp
// Linux-runnable unit tests for engine/scripting/script_abi_layout.h.
//
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/script_abi_layout_test tests/script_abi_layout_test.cpp
//   /tmp/script_abi_layout_test

#include "engine/scripting/script_abi_layout.h"

#include <cstdint>
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

static bool Contains(const std::string& text, const char* needle) {
    return text.find(needle) != std::string::npos;
}

int main() {
    using namespace script_abi;

    std::vector<std::uint32_t> host(kEntryCount);
    for (std::uint32_t i = 0; i < kEntryCount; ++i)
        host[i] = 100 + i;
    host[kRevision] = kAbiRevision;
    host[kIteratorDebugLevel] = 0;
    host[kDebugCrt] = 0;

    // Identical layouts.
    {
        auto dll = host;
        Expect(Compare(host.data(), host.size(), dll.data(), dll.size()).empty(), "same layout -> no mismatch");
        Expect(Describe({}, "x.dll").empty(), "no mismatch -> empty text");
    }

    // One engine class grew: header-change wording, entry named.
    {
        auto dll = host;
        dll[kSizeofGameObject] += 8;
        const auto m = Compare(host.data(), host.size(), dll.data(), dll.size());
        Expect(m.size() == 1 && m[0].index == kSizeofGameObject, "GameObject size mismatch found");
        Expect(m[0].host == host[kSizeofGameObject] && m[0].dll == host[kSizeofGameObject] + 8, "values kept");
        Expect(!IsBuildFlavourMismatch(m), "size-only mismatch is not a flavour mismatch");
        const std::string text = Describe(m, "game/scripts/bin/GameScripts.dll");
        Expect(Contains(text, "not loading game/scripts/bin/GameScripts.dll"), "text names the DLL");
        Expect(Contains(text, "engine headers changed"), "text gives the rebuild cause");
        Expect(Contains(text, "sizeof(GameObject): engine "), "text names the entry");
    }

    // Debug DLL in a Release host: flavour wording wins even with size diffs.
    {
        auto dll = host;
        dll[kIteratorDebugLevel] = 2;
        dll[kDebugCrt] = 1;
        dll[kSizeofString] += 8;
        const auto m = Compare(host.data(), host.size(), dll.data(), dll.size());
        Expect(m.size() == 3, "three mismatches");
        Expect(IsBuildFlavourMismatch(m), "flavour mismatch detected");
        Expect(Contains(Describe(m, "a.dll"), "Debug / Release mismatch"), "flavour wording");
    }

    // Manual revision bump.
    {
        auto dll = host;
        dll[kRevision] = kAbiRevision + 1;
        const auto m = Compare(host.data(), host.size(), dll.data(), dll.size());
        Expect(m.size() == 1 && m[0].index == kRevision, "revision mismatch found");
        Expect(Contains(Describe(m, "a.dll"), "ABI revision: engine"), "revision named");
    }

    // Different entry counts: only the shared prefix is compared.
    {
        auto shorter = host;
        shorter.resize(kSizeofGameObject);  // older DLL without the later entries
        Expect(Compare(host.data(), host.size(), shorter.data(), shorter.size()).empty(),
               "older DLL with matching prefix -> no mismatch");
        auto longer = host;
        longer.push_back(12345);  // newer DLL with an extra entry
        Expect(Compare(host.data(), host.size(), longer.data(), longer.size()).empty(),
               "newer DLL extra entry ignored");
        shorter[kPointerSize] = 4;
        Expect(Compare(host.data(), host.size(), shorter.data(), shorter.size()).size() == 1,
               "mismatch inside the shared prefix still found");
    }

    // Null inputs and names.
    Expect(Compare(nullptr, 3, host.data(), host.size()).empty(), "null host -> nothing");
    Expect(Compare(host.data(), host.size(), nullptr, 3).empty(), "null dll -> nothing");
    for (std::uint32_t i = 0; i < kEntryCount; ++i)
        Expect(std::strcmp(EntryName(i), "unknown entry") != 0, "every entry has a name");
    Expect(std::strcmp(EntryName(kEntryCount), "unknown entry") == 0, "out of range name");

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("script_abi_layout_test: all checks passed\n");
    return 0;
}
