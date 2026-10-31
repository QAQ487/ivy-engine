// scene_execution_policy.h
// Declares SceneExecutionPolicy.

#pragma once
#include "libraries/nlohmann/json.hpp"

struct SceneExecutionPolicy {
    bool scene2D = false;
    bool physics3D = true;
    bool physics2D = false;
    static SceneExecutionPolicy FromSettings(const nlohmann::json& settings) {
        if(!settings.is_object()) return {};
        SceneExecutionPolicy policy;
        policy.scene2D = settings.value("renderPath",std::string("3d"))=="2d";
        policy.physics3D = !policy.scene2D && settings.value("physicsMode",std::string("3d"))=="3d";
        policy.physics2D = policy.scene2D && settings.value("physicsMode",std::string("none"))=="2d";
        return policy;
    }
    void WriteSettings(nlohmann::json& settings) const {
        settings["renderPath"]=scene2D?"2d":"3d";
        settings["physicsMode"]=scene2D && physics2D?"2d":!scene2D && physics3D?"3d":"none";
    }
};
