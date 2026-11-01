// json_float_test.cpp
// Linux-runnable unit tests for scene/prefab float JSON formatting and
// dirty/no-op edit detection. No Windows headers.
//
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/json_float_test tests/json_float_test.cpp

#include "engine/scene/json_float.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "libraries/nlohmann/json.hpp"

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static void ExpectEq(const std::string& got, const char* want, const char* msg) {
    if (got != want) {
        std::fprintf(stderr, "FAIL: %s (got '%s', want '%s')\n", msg, got.c_str(), want);
        ++g_failed;
    }
}

static void TestFormatRoundingAndTrim() {
    ExpectEq(JsonFloat_Format(1.5f), "1.5", "trim trailing zeros 1.5");
    ExpectEq(JsonFloat_Format(1.500f), "1.5", "trim trailing zeros 1.500");
    ExpectEq(JsonFloat_Format(1.0f), "1", "integer trim");
    ExpectEq(JsonFloat_Format(0.0f), "0", "zero");
    ExpectEq(JsonFloat_Format(-0.0f), "0", "normalize -0");
    ExpectEq(JsonFloat_Format(1.2346f), "1.235", "round half up at 3dp");
    ExpectEq(JsonFloat_Format(1.2344f), "1.234", "round down at 3dp");
    ExpectEq(JsonFloat_Format(-1.2346f), "-1.235", "negative round");
    ExpectEq(JsonFloat_Format(10.0f), "10", "large integer");
    ExpectEq(JsonFloat_Format(12345.6789f), "12345.679", "large with decimals");
    ExpectEq(JsonFloat_Format(1e-6f), "0", "tiny rounds to 0");
    ExpectEq(JsonFloat_Format(0.10000000149011612f), "0.1", "binary float noise 0.1");
    ExpectEq(JsonFloat_Format(0.8999999761581421f), "0.9", "binary float noise 0.9");
}

static void TestFormatNonFinite() {
    ExpectEq(JsonFloat_Format(std::nanf("")), "null", "NaN -> null");
    ExpectEq(JsonFloat_Format(INFINITY), "null", "Inf -> null");
    ExpectEq(JsonFloat_Format(-INFINITY), "null", "-Inf -> null");
}

static void TestDumpUsesRoundedFloats() {
    nlohmann::json j = {
        {"posX", 0.10000000149011612},
        {"rotY", -0.0},
        {"nested", {{"a", 1.23456}, {"b", 2}}},
        {"arr", {1.5, 0.8999999761581421}}
    };
    const std::string out = JsonFloat_Dump(j, 4);
    Expect(out.find("0.10000000149011612") == std::string::npos, "dump drops noisy 0.1");
    Expect(out.find("\"posX\": 0.1\n") != std::string::npos ||
           out.find("\"posX\": 0.1,") != std::string::npos, "dump writes 0.1");
    Expect(out.find("\"rotY\": -0") == std::string::npos, "dump normalizes -0");
    Expect(out.find("\"rotY\": 0\n") != std::string::npos ||
           out.find("\"rotY\": 0,") != std::string::npos, "dump writes 0 for -0");
    Expect(out.find("\"a\": 1.235") != std::string::npos, "nested rounded");
    Expect(out.find("1.5") != std::string::npos, "array trimmed");
    Expect(out.find("0.9") != std::string::npos &&
           out.find("0.8999999761581421") == std::string::npos, "array noisy float rounded");
    // Still valid JSON after custom dump.
    const auto parsed = nlohmann::json::parse(out);
    Expect(std::abs(parsed["posX"].get<double>() - 0.1) < 1e-9, "roundtrip parse posX");
    Expect(parsed["nested"]["b"] == 2, "ints preserved");
}

static void TestDirtyNoOpLogic() {
    Expect(JsonFloat_Equal(1.0f, 1.0f), "equal exact");
    Expect(JsonFloat_Equal(0.100000001f, 0.1f), "equal within 3dp");
    Expect(!JsonFloat_Equal(1.0f, 1.002f), "differ beyond 3dp");
    Expect(JsonFloat_Equal(-0.0f, 0.0f), "-0 equals 0");

    // 1.2344 -> 1.234, 1.2346 -> 1.235 => changed at 3dp
    Expect(JsonFloat_Changed(1.2344f, 1.2346f), "3dp boundary counts as change");
    Expect(!JsonFloat_Changed(1.2341f, 1.2344f), "same 3dp bucket is no-op");

    const float before[9] = {1.f, 2.f, 3.f, 10.f, 20.f, 30.f, 1.f, 1.f, 1.f};
    float afterSame[9] = {1.0000001f, 2.f, 3.f, 10.f, 20.f, 30.f, 1.f, 1.f, 1.f};
    float afterEdit[9] = {1.f, 2.f, 3.1f, 10.f, 20.f, 30.f, 1.f, 1.f, 1.f};
    Expect(!JsonFloat_TransformChanged(before, afterSame), "transform noise is no-op");
    Expect(JsonFloat_TransformChanged(before, afterEdit), "transform real edit dirties");

    Expect(JsonFloat_JsonEqual(
        R"({"posX":0.1,"rotY":-0.0})",
        R"({"posX":0.10000000149011612,"rotY":0.0})"),
        "json equal ignores float noise and -0");
    Expect(!JsonFloat_JsonEqual(
        R"({"posX":1.0})",
        R"({"posX":1.002})"),
        "json unequal beyond 3dp");
}

int main() {
    TestFormatRoundingAndTrim();
    TestFormatNonFinite();
    TestDumpUsesRoundedFloats();
    TestDirtyNoOpLogic();

    if (g_failed) {
        std::fprintf(stderr, "json_float_test: %d failure(s)\n", g_failed);
        return 1;
    }
    std::printf("json_float_test PASS\n");
    return 0;
}
