#pragma once

#include "ui_canvas_script.h"
#include <string>

#if defined(SCRIPT_DLL_BUILD) || defined(_WINDLL)

#include <functional>

void DllUICanvasScript_Register(const std::string& name, std::function<UICanvasScript*()> creator);

#define REGISTER_UI_SCRIPT(TYPE)                                     \
    namespace {                                                      \
        struct TYPE##_DllUiRegistrator {                             \
            TYPE##_DllUiRegistrator() {                              \
                DllUICanvasScript_Register(#TYPE,                    \
                    []() -> UICanvasScript* { return new TYPE(); }); \
            }                                                        \
        } TYPE##_dllUiRegistratorInstance;                           \
    }

#else

#include "ui_canvas_script_factory.h"

#define REGISTER_UI_SCRIPT(TYPE)                                              \
    namespace {                                                               \
        struct TYPE##_UiScriptRegistrator {                                   \
            TYPE##_UiScriptRegistrator() {                                    \
                UICanvasScriptFactory::Instance().Register(                   \
                    #TYPE,                                                    \
                    []() -> UICanvasScriptFactory::ScriptPtr {                \
                        return UICanvasScriptFactory::ScriptPtr(              \
                            new TYPE(),                                       \
                            [](UICanvasScript* script) { delete script; });   \
                    });                                                       \
            }                                                                 \
        } TYPE##_uiScriptRegistratorInstance;                                 \
    }

#endif
