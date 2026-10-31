// SampleCheckpointState.h
// Declares SampleCheckpointLatch.

#pragma once
#include <cstring>

// Tiny pure latch for once-only Player checkpoint scoring (unit-testable without physics).
struct SampleCheckpointLatch {
    bool achieved = false;
    int score = 0;

    bool TryAcceptPlayer(const char* actorName) {
        if (actorName == nullptr || std::strcmp(actorName, "Player") != 0)
            return false;
        if (achieved)
            return false;
        achieved = true;
        score = 1;
        return true;
    }
};
