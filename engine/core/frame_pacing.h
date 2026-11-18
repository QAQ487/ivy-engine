// frame_pacing.h
// Pure logic for the WinMain frame gate: how long the loop may block before
// the next frame is due. The loop blocks for most of the gap (so it does not
// spin a CPU core at 100%) and spins only the last spinMarginSec, because a
// timed wait can wake a millisecond or two late even with timeBeginPeriod(1),
// and the 3D sim advances a fixed 1/60 s per frame, so a late frame is slow
// motion. No Windows dependency, so it is unit-testable on Linux.

#pragma once

namespace frame_pacing {

// Frame interval of the main loop gate (src/app/main.cpp).
inline constexpr double kFrameSeconds = 1.0 / 60.0;

// Part of each gap left to a short spin instead of a timed wait.
inline constexpr double kSpinMarginSeconds = 0.002;

// Whole milliseconds the loop may block, given the time since the last frame.
// Returns 0 when the frame is due or due within spinMarginSec (spin instead),
// and also for non-finite or negative inputs.
inline unsigned WaitMilliseconds(double elapsedSec,
                                 double frameSec = kFrameSeconds,
                                 double spinMarginSec = kSpinMarginSeconds) {
    // NaN fails every comparison, so test the "good" direction.
    if (!(elapsedSec >= 0.0) || !(frameSec > 0.0) || !(spinMarginSec >= 0.0))
        return 0;
    const double blockSec = frameSec - elapsedSec - spinMarginSec;
    if (!(blockSec >= 0.001))
        return 0;
    // frameSec bounds blockSec, so a huge frameSec is the only overflow risk.
    if (blockSec >= 1000.0)
        return 1000u * 1000u;
    return static_cast<unsigned>(blockSec * 1000.0);
}

}  // namespace frame_pacing
