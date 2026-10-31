#pragma once

#include "engine/scripting/Script.h"
#include <string>

// Use this header from game script .cpp files. When the file is compiled into a
// script DLL, scripts register into the DLL-local export registry. When the same
// file is compiled into the main game project, scripts keep the old static
// registration path so the game still works without external DLLs.
#if defined(SCRIPT_DLL_BUILD) || defined(_WINDLL)

#include <functional>

void DllScript_Register(const std::string& name, std::function<Script*()> creator);

#define REGISTER_SCRIPT(TYPE)                                        \
    namespace {                                                      \
        struct TYPE##_DllRegistrator {                               \
            TYPE##_DllRegistrator() {                                \
                DllScript_Register(#TYPE,                            \
                    []() -> Script* { return new TYPE(); });         \
            }                                                        \
        } TYPE##_dllRegistratorInstance;                             \
    }

#else

#include "script_factory.h"

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

#endif