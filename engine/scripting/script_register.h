/*****************************************************
 * File: engine/scripting/ScriptRegistrar.h
 * Project: d3d11_engine
 * Author: Terry
 * Created: 2026-03-13
 * Description: ScriptRegistrar.h – convenience macro for self-registering a
 *              Script subclass with ScriptFactory at static-init time.
 *
 * Usage: place REGISTER_SCRIPT(MyScript) in the script's .cpp file so it runs
 * before main() — no centralized registration list is needed.
 *
 *   // MyScript.cpp
 *   #include "MyScript.h"
 *   #include "engine/scripting/ScriptRegistrar.h"
 *
 *   REGISTER_SCRIPT(MyScript)
 *
 * The class name string (e.g. "MyScript") is what goes into prefab JSON:
 *   "Script": { "scriptClass": "MyScript" }
 * Serialized fields are declared in the script header via script_fields.h.
 *****************************************************/
#pragma once
#include "script_factory.h"

// REGISTER_SCRIPT(TYPE) – self-registers TYPE with ScriptFactory under the
// stringified class name at static-init time.  Place once in the script's
// .cpp file.
#define REGISTER_SCRIPT(TYPE)                                               \
    namespace {                                                             \
        struct TYPE##_ScriptRegistrator {                                   \
            TYPE##_ScriptRegistrator() {                                    \
                ScriptFactory::Instance().Register(                         \
                    #TYPE,                                                  \
                    []() -> ScriptFactory::ScriptPtr {                      \
                        return ScriptFactory::ScriptPtr(                     \
                            new TYPE(),                                     \
                            [](Script* script) { delete script; });         \
                    });                                                     \
            }                                                               \
        } TYPE##_scriptRegistratorInstance;                                 \
    }
