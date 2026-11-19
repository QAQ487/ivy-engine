// input_action_system.h
// Process-wide input actions for the script API (ScriptApi_Input*).
//
// Defaults: project.json input.actions (working directory, then beside the exe).
// Player rebinding: save slot "ivy_input", key "bindings" (save_system.h), written
// only by InputActionSystem_SaveOverrides and applied on top of the defaults.
// Raw input: keyboard, pad 0 and mouse buttons, sampled once per frame by
// InputActionSystem_Update after KeyLogger_Update / Controller_Update.
//
// Lock InputActionSystem_Mutex() around InputActionSystem_Set() / _LastSnapshot().
// Lock order: this mutex before SaveSystem_Mutex().

#pragma once

#include <mutex>
#include <string>

namespace input_actions {
class ActionSet;
struct Snapshot;
}

inline constexpr const char* kInputSaveSlot = "ivy_input";

std::mutex& InputActionSystem_Mutex();
// Loaded on first use. Caller holds the mutex.
input_actions::ActionSet& InputActionSystem_Set();
const input_actions::Snapshot& InputActionSystem_LastSnapshot();

// Main thread, once per frame: sample raw input and update every action.
void InputActionSystem_Update();
// Re-read project.json defaults and the saved rebinding (drops unsaved runtime
// changes). The editor calls it after the Input page saves, when a project is
// opened and when Play stops.
void InputActionSystem_ReloadDefaults();
// Writes the differences from the defaults to the save slot and commits it.
bool InputActionSystem_SaveOverrides(std::string* error);
// Logs "[Input] unknown action" once per name. Caller holds the mutex.
void InputActionSystem_WarnUnknown(const char* name);
