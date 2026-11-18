// Light selection math tests.
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/light_select_test tests/light_select_test.cpp

#include "engine/rendering/culling/light_select_math.h"

#include <cstdio>
#include <vector>

using namespace light_select;

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static Candidate Dir() { Candidate c; c.directional = true; return c; }
static Candidate Pt(float x, float range = 10.0f, float luma = 1.0f, bool was = false) {
    Candidate c; c.x = x; c.range = range; c.luma = luma; c.wasSelected = was; return c;
}

int main() {
    int out[8];

    // Fewer lights than slots: all used, scene order.
    {
        std::vector<Candidate> v = {Pt(100), Dir(), Pt(0)};
        const int n = Select(v.data(), 3, 0, 0, 0, 4, out);
        Expect(n == 3 && out[0] == 0 && out[1] == 1 && out[2] == 2, "<= max keeps all in order");
    }
    // Exactly max: unchanged.
    {
        std::vector<Candidate> v = {Pt(1), Pt(2), Pt(3), Pt(4)};
        Expect(Select(v.data(), 4, 0, 0, 0, 4, out) == 4 && out[3] == 3, "== max keeps all");
    }
    // Directional always wins, nearest points fill the rest; output ascending.
    {
        std::vector<Candidate> v = {Pt(90), Pt(80), Pt(70), Pt(1), Dir(), Pt(2), Pt(60)};
        const int n = Select(v.data(), 7, 0, 0, 0, 4, out);
        Expect(n == 4, "returns maxOut");
        Expect(out[0] == 3 && out[1] == 4 && out[2] == 5 && out[3] == 6,
               "dir + nearest 3 points (1, 2, 60), scene order");
    }
    // Brighter far light can beat a dim near one.
    {
        std::vector<Candidate> v = {Pt(5, 10, 0.05f), Pt(8, 10, 5.0f), Pt(50, 10, 0.1f)};
        const int n = Select(v.data(), 3, 0, 0, 0, 1, out);
        Expect(n == 1 && out[0] == 1, "bright light wins over dim near light");
    }
    // Bigger range reaches further.
    {
        std::vector<Candidate> v = {Pt(40, 5.0f), Pt(40, 80.0f)};
        Expect(Select(v.data(), 2, 0, 0, 0, 1, out) == 1 && out[0] == 1, "larger range wins");
    }
    // Hysteresis: previous winner keeps its slot on a near tie.
    {
        std::vector<Candidate> v = {Pt(10.0f, 10, 1.0f, true), Pt(9.5f, 10, 1.0f, false)};
        Expect(Select(v.data(), 2, 0, 0, 0, 1, out) == 1 && out[0] == 0, "sticky winner on near tie");
        v[0].wasSelected = false;
        Select(v.data(), 2, 0, 0, 0, 1, out);
        Expect(out[0] == 1, "without bonus the nearer light wins");
    }
    // Ties: stable, lower index first.
    {
        std::vector<Candidate> v = {Pt(10), Pt(10), Pt(10)};
        Select(v.data(), 3, 0, 0, 0, 2, out);
        Expect(out[0] == 0 && out[1] == 1, "ties keep scene order");
    }
    // Bad numbers never crash or win by accident.
    {
        Candidate bad = Pt(0); bad.luma = NAN; bad.range = INFINITY; bad.x = INFINITY;
        std::vector<Candidate> v = {bad, Pt(5)};
        Expect(Select(v.data(), 2, 0, 0, 0, 1, out) == 1 && out[0] == 1, "non-finite light loses");
    }
    // Degenerate inputs.
    Expect(Select(nullptr, 3, 0, 0, 0, 4, out) == 0, "null lights -> 0");
    Expect(Select(nullptr, 0, 0, 0, 0, 4, out) == 0, "empty -> 0");
    {
        std::vector<Candidate> v = {Pt(1)};
        Expect(Select(v.data(), 1, 0, 0, 0, 0, out) == 0, "maxOut 0 -> 0");
    }

    if (g_failed == 0)
        std::puts("light_select_test: ok");
    return g_failed == 0 ? 0 : 1;
}
