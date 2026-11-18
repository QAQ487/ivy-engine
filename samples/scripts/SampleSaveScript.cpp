// SampleSaveScript.cpp
// Sample script: SampleSaveScript implementation.

#include "SampleSaveScript.h"

#include "engine/input/key_logger.h"
#include "engine/input/keyboard.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "libraries/nlohmann/json.hpp"

#include <string>

REGISTER_SCRIPT(SampleSaveScript)

void SampleSaveScript::OnCreate(engine::GameObject& /*self*/) {
    // Every Play session bumps "plays" and commits it straight away.
    m_plays = ScriptApi_SaveGetInt(slot.c_str(), "plays", 0) + 1;
    m_counter = ScriptApi_SaveGetInt(slot.c_str(), "counter", 0);
    ScriptApi_SaveSetInt(slot.c_str(), "plays", m_plays);
    const bool saved = ScriptApi_SaveCommit(slot.c_str());
    Refresh(saved ? "Loaded" : "Save failed (see Console)");
}

void SampleSaveScript::OnUpdate(engine::GameObject& /*self*/, float /*dt*/) {
    if (KeyLogger_IsTrigger(KK_K)) {
        ++m_counter;
        ScriptApi_SaveSetInt(slot.c_str(), "counter", m_counter);
        // Nested data goes in as JSON text.
        nlohmann::json last = {{"counter", m_counter}, {"realTime", ScriptApi_GetRealTimeSeconds()}};
        ScriptApi_SaveSetJson(slot.c_str(), "lastSave", last.dump().c_str());
        Refresh(ScriptApi_SaveCommit(slot.c_str()) ? "Saved" : "Save failed (see Console)");
    }
    if (KeyLogger_IsTrigger(KK_L)) {
        const bool deleted = ScriptApi_SaveDeleteSlot(slot.c_str());
        m_plays = 0;
        m_counter = 0;
        Refresh(deleted ? "Slot deleted" : "Delete failed (see Console)");
    }
}

void SampleSaveScript::Refresh(const char* status) {
    UICanvas* canvas = ScriptApi_SceneCanvasGetCanvas();
    if (!canvas)
        return;
    auto* text = dynamic_cast<UIText*>(ScriptApi_UICanvasGetElement(*canvas, textElement.c_str()));
    if (!text)
        return;
    const std::string line = "Slot '" + slot + "'  plays: " + std::to_string(m_plays) +
                             "  counter: " + std::to_string(m_counter) + "  [" + status + "]";
    ScriptApi_UITextSetText(*text, line.c_str());
}
