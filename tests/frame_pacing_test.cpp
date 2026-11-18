// frame_pacing_test.cpp
// Linux-runnable unit tests for engine/core/frame_pacing.h.
//
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/frame_pacing_test tests/frame_pacing_test.cpp
//   /tmp/frame_pacing_test

#include "engine/core/frame_pacing.h"

#include <cstdio>
#include <limits>

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

int main() {
    using frame_pacing::WaitMilliseconds;
    const double frame = frame_pacing::kFrameSeconds;
    const double margin = frame_pacing::kSpinMarginSeconds;

    // Right after a frame: block for the gap minus the spin margin (16.67 - 2 -> 14 ms).
    Expect(WaitMilliseconds(0.0) == 14, "fresh frame waits 14 ms");
    // Never blocks past the frame deadline minus the margin.
    for (int i = 0; i <= 200; ++i) {
        const double elapsed = frame * i / 200.0;
        const unsigned ms = WaitMilliseconds(elapsed);
        if (ms > 0)
            Expect(elapsed + ms / 1000.0 <= frame - margin + 1e-12, "wait ends before deadline - margin");
    }
    // Inside the spin margin or past the deadline: do not block.
    Expect(WaitMilliseconds(frame - margin) == 0, "at margin -> spin");
    Expect(WaitMilliseconds(frame - 0.0005) == 0, "inside margin -> spin");
    Expect(WaitMilliseconds(frame) == 0, "due -> no wait");
    Expect(WaitMilliseconds(frame * 3.0) == 0, "late -> no wait");
    // Less than one whole millisecond left to block -> spin.
    Expect(WaitMilliseconds(frame - margin - 0.0009) == 0, "sub-millisecond block -> spin");
    Expect(WaitMilliseconds(frame - margin - 0.0011) == 1, "just over 1 ms -> 1");

    // Custom frame interval and margin.
    Expect(WaitMilliseconds(0.0, 0.1, 0.0) == 100, "100 ms frame, no margin");
    Expect(WaitMilliseconds(0.05, 0.1, 0.01) == 40, "half elapsed, 10 ms margin");

    // Bad inputs never block.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    Expect(WaitMilliseconds(-0.001) == 0, "negative elapsed");
    Expect(WaitMilliseconds(nan) == 0, "NaN elapsed");
    Expect(WaitMilliseconds(inf) == 0, "inf elapsed");
    Expect(WaitMilliseconds(0.0, 0.0) == 0, "zero frame");
    Expect(WaitMilliseconds(0.0, -1.0) == 0, "negative frame");
    Expect(WaitMilliseconds(0.0, nan) == 0, "NaN frame");
    Expect(WaitMilliseconds(0.0, frame, -0.001) == 0, "negative margin");
    Expect(WaitMilliseconds(0.0, frame, nan) == 0, "NaN margin");
    Expect(WaitMilliseconds(0.0, inf, 0.0) == 1000u * 1000u, "inf frame clamps");

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("frame_pacing_test: all checks passed\n");
    return 0;
}
