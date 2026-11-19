// input_action_system.cpp
// Windows layer for input actions: raw input sampling, project.json defaults
// and saved rebinding. See input_action_system.h.

#include "engine/input/input_action_system.h"
#include "engine/input/input_actions.h"
#include "engine/input/controller.h"
#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"
#include "engine/save/save_store.h"
#include "engine/save/save_system.h"
#include "engine/debug_draw/debug_ofstream.h"
#include "libraries/nlohmann/json.hpp"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

input_actions::ActionSet g_set;
input_actions::Snapshot g_snapshot;
bool g_loaded = false;
std::set<std::string, std::less<>> g_warned;

constexpr ControllerButton kPadButtons[] = {
    ControllerButton::DPadUp, ControllerButton::DPadDown, ControllerButton::DPadLeft, ControllerButton::DPadRight,
    ControllerButton::Start,  ControllerButton::Back,     ControllerButton::LeftThumb, ControllerButton::RightThumb,
    ControllerButton::LB,     ControllerButton::RB,       ControllerButton::A,         ControllerButton::B,
    ControllerButton::X,      ControllerButton::Y,
};

void LogLine(const std::string& line) {
    hal::DebugWriteA(("[Input] " + line + "\n").c_str());
}

void LogIssues(const std::vector<input_actions::Issue>& issues) {
    for (const auto& i : issues)
        LogLine((i.action.empty() ? std::string() : "action '" + i.action + "': ") + i.message);
}

fs::path ExeDirectory() {
    wchar_t buf[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return (len > 0 && len < MAX_PATH) ? fs::path(buf).parent_path() : fs::path();
}

// input.actions from project.json in the working directory, else beside the exe.
json ReadProjectActions() {
    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    for (const fs::path& candidate : {cwd / "project.json", ExeDirectory() / "project.json"}) {
        std::ifstream in(candidate, std::ios::binary);
        if (!in)
            continue;
        const json j = json::parse(in, nullptr, false);
        if (j.is_discarded() || !j.is_object()) {
            LogLine("project.json is not valid JSON; no input actions loaded");
            return json();
        }
        const auto input = j.find("input");
        if (input == j.end() || !input->is_object())
            return json();
        const auto actions = input->find("actions");
        return actions == input->end() ? json() : *actions;
    }
    return json();
}

void LoadLocked() {
    std::vector<input_actions::Issue> issues;
    std::vector<input_actions::ActionDef> defs = input_actions::ParseActions(ReadProjectActions(), &issues);
    g_set.SetDefaults(std::move(defs), &issues);
    LogIssues(issues);

    std::string savedText;
    {
        std::lock_guard<std::mutex> saveLock(SaveSystem_Mutex());
        SaveSystem_Store().GetJsonText(kInputSaveSlot, "bindings", &savedText);
    }
    if (!savedText.empty()) {
        issues.clear();
        const json saved = json::parse(savedText, nullptr, false);
        g_set.ApplyOverrides(saved, &issues);
        LogIssues(issues);
    }
    g_set.Update(g_snapshot);
    g_warned.clear();
    g_loaded = true;
}

void Sample(input_actions::Snapshot& s) {
    for (int k = 1; k < 256; ++k)
        s.keys[k] = Keyboard_IsKeyDown(static_cast<Keyboard_Keys>(k));
    s.padConnected = Controller_IsConnected(0);
    s.padButtons = 0;
    if (s.padConnected) {
        for (ControllerButton b : kPadButtons) {
            if (Controller_IsPressed(b, 0))
                s.padButtons = static_cast<std::uint16_t>(s.padButtons | static_cast<WORD>(b));
        }
    }
    const DirectX::XMFLOAT2 left = Controller_GetLeftStick(0);
    const DirectX::XMFLOAT2 right = Controller_GetRightStick(0);
    s.leftX = left.x;
    s.leftY = left.y;
    s.rightX = right.x;
    s.rightY = right.y;
    s.leftTrigger = Controller_GetLeftTrigger(0);
    s.rightTrigger = Controller_GetRightTrigger(0);
    s.mouse[0] = Mouse_IsLeftDown();
    s.mouse[1] = Mouse_IsRightDown();
    s.mouse[2] = Mouse_IsMiddleDown();
}

}  // namespace

std::mutex& InputActionSystem_Mutex() {
    static std::mutex m;
    return m;
}

input_actions::ActionSet& InputActionSystem_Set() {
    if (!g_loaded)
        LoadLocked();
    return g_set;
}

const input_actions::Snapshot& InputActionSystem_LastSnapshot() {
    return g_snapshot;
}

void InputActionSystem_Update() {
    input_actions::Snapshot s;
    Sample(s);
    std::lock_guard<std::mutex> lock(InputActionSystem_Mutex());
    g_snapshot = s;
    InputActionSystem_Set().Update(s);
}

void InputActionSystem_ReloadDefaults() {
    std::lock_guard<std::mutex> lock(InputActionSystem_Mutex());
    LoadLocked();
}

bool InputActionSystem_SaveOverrides(std::string* error) {
    std::string text;
    {
        std::lock_guard<std::mutex> lock(InputActionSystem_Mutex());
        text = InputActionSystem_Set().OverridesToJson().dump();
    }
    std::lock_guard<std::mutex> saveLock(SaveSystem_Mutex());
    save_store::Store& store = SaveSystem_Store();
    if (!store.SetJsonText(kInputSaveSlot, "bindings", text) || !store.Commit(kInputSaveSlot)) {
        if (error)
            *error = "could not write the input save slot (see Console)";
        return false;
    }
    return true;
}

void InputActionSystem_WarnUnknown(const char* name) {
    const std::string key = name ? name : "";
    if (g_warned.insert(key).second)
        LogLine("unknown action '" + key + "' (not in project.json input.actions)");
}
