// runtime_bridge_checkpoint_check.cpp
// Validates SampleCheckpointLatch one-shot / wrong-actor checks.

// Pure SampleCheckpointLatch one-shot / wrong-actor checks (no physics).
#include "samples/scripts/SampleCheckpointState.h"
#include <iostream>

static void check(bool b, const char* label) {
    if (!b) {
        std::cerr << "FAIL " << label << std::endl;
        std::exit(1);
    }
}

int main() {
    SampleCheckpointLatch latch;
    check(!latch.TryAcceptPlayer(nullptr), "null actor ignored");
    check(!latch.TryAcceptPlayer("Enemy"), "wrong actor ignored");
    check(!latch.achieved && latch.score == 0, "still unlatched");
    check(latch.TryAcceptPlayer("Player"), "Player accepted once");
    check(latch.achieved && latch.score == 1, "achieved score 1");
    check(!latch.TryAcceptPlayer("Player"), "reentry duplicate rejected");
    check(latch.score == 1, "score stays 1");
    std::cout << "PASS checkpoint latch wrong-actor/one-shot\n";
    return 0;
}
