// SampleRebindScript.h
// Declares SampleRebindScript.

#pragma once
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"

#include <string>

// Input action rebinding demo (ScriptApi_Input*, ScriptRuntimeApi v52).
// Shows an action's bindings and whether it is pressed in a canvas text.
// R: press-a-key rebinding of the first binding. Backspace: back to the
// project defaults. Enter: save the player's bindings.
class SampleRebindScript : public Script {
public:
#define SAMPLE_REBIND_FIELDS(X) \
    X(std::string, action, "Jump") \
    X(std::string, textElement, "RebindStatus")

    SAMPLE_REBIND_FIELDS(SCRIPT_DECLARE_FIELD)
    SCRIPT_SERIALIZE_METHODS(SAMPLE_REBIND_FIELDS)

    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;

private:
    void Refresh();

    bool m_capturing = false;
    std::string m_status = "R: rebind   Backspace: defaults   Enter: save";
};
