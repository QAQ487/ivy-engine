#pragma once

#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"

class BallRunAnimTestScript : public Script {
#define BALL_RUN_ANIM_TEST_SCRIPT_FIELDS(X) \
    X(std::string, runClip, "run") \
    X(std::string, danceClip, "dance")\
	X(std::string, jumpClip, "jump")\
    X(std::string, pushClip, "push")\
    X(std::string, pullClip, "pull")\
    X(std::string, throwClip, "throw")\

public:
    BALL_RUN_ANIM_TEST_SCRIPT_FIELDS(SCRIPT_DECLARE_FIELD)

    void OnUpdate(engine::GameObject& self, float dt) override;

    SCRIPT_SERIALIZE_METHODS(BALL_RUN_ANIM_TEST_SCRIPT_FIELDS)
};
