// SampleSaveScript.h
// Declares SampleSaveScript.

#pragma once
#include "engine/scripting/Script.h"
#include "engine/scripting/script_fields.h"

#include <string>

// Save-data demo (ScriptApi_Save*, ScriptRuntimeApi v51). Counts Play
// sessions and keeps a counter in one save slot, shown in a canvas text
// element. K: counter + 1 and commit. L: delete the slot.
class SampleSaveScript : public Script {
public:
#define SAMPLE_SAVE_FIELDS(X) \
    X(std::string, slot, "demo") \
    X(std::string, textElement, "SaveStatus")

    SAMPLE_SAVE_FIELDS(SCRIPT_DECLARE_FIELD)
    SCRIPT_SERIALIZE_METHODS(SAMPLE_SAVE_FIELDS)

    void OnCreate(engine::GameObject& self) override;
    void OnUpdate(engine::GameObject& self, float dt) override;

private:
    void Refresh(const char* status);

    long long m_plays = 0;
    long long m_counter = 0;
};
