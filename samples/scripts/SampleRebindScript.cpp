// SampleRebindScript.cpp
// Sample script: SampleRebindScript implementation.

#include "SampleRebindScript.h"

#include "engine/input/key_logger.h"
#include "engine/input/keyboard.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"

#include <string>

REGISTER_SCRIPT(SampleRebindScript)

void SampleRebindScript::OnCreate(engine::GameObject& /*self*/) {
    if (!ScriptApi_InputHasAction(action.c_str()))
        m_status = "Action '" + action + "' is not in project.json input.actions";
    Refresh();
}

void SampleRebindScript::OnUpdate(engine::GameObject& /*self*/, float /*dt*/) {
    if (m_capturing) {
        std::string binding;
        const ScriptInputCaptureState state = ScriptApi_InputCaptureState(&binding);
        if (state == ScriptInputCaptureState::Listening) {
            Refresh();
            return;
        }
        m_capturing = false;
        m_status = state == ScriptInputCaptureState::Done ? "Bound to " + binding + " (Enter saves it)"
                                                          : "Rebind cancelled";
        // The key that finished the capture is still down this frame: handle keys next frame.
        Refresh();
        return;
    }

    if (KeyLogger_IsTrigger(KK_R)) {
        if (ScriptApi_InputCaptureStart(action.c_str(), 0)) {
            m_capturing = true;
            m_status = "Press a key, pad button or mouse button for '" + action + "' (Esc cancels)";
        } else {
            m_status = "Cannot rebind (see Console)";
        }
    } else if (KeyLogger_IsTrigger(KK_BACK)) {
        m_status = ScriptApi_InputResetBindings(action.c_str()) ? "Back to the project defaults (Enter saves it)"
                                                                : "Reset failed (see Console)";
    } else if (KeyLogger_IsTrigger(KK_ENTER)) {
        m_status = ScriptApi_InputSaveBindings() ? "Saved: loaded again on the next Play"
                                                 : "Save failed (see Console)";
    }
    Refresh();
}

void SampleRebindScript::Refresh() {
    UICanvas* canvas = ScriptApi_SceneCanvasGetCanvas();
    if (!canvas)
        return;
    auto* text = dynamic_cast<UIText*>(ScriptApi_UICanvasGetElement(*canvas, textElement.c_str()));
    if (!text)
        return;
    std::string bindings;
    for (const std::string& b : ScriptApi_InputGetBindings(action.c_str()))
        bindings += (bindings.empty() ? "" : ", ") + b;
    const std::string line = action + ": " + (bindings.empty() ? "(none)" : bindings) + "   pressed: " +
                             (ScriptApi_InputIsPressed(action.c_str()) ? "YES" : "no") + "\n" + m_status;
    ScriptApi_UITextSetText(*text, line.c_str());
}
