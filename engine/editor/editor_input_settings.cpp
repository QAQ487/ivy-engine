// editor_input_settings.cpp
// Project Settings > Input: edit project.json input.actions (names, types,
// bindings), with a Listen button that records the next key / pad input.
// Like the Physics and Audio pages, a valid change is written to project.json
// straight away (no undo); a set with problems is shown but not written.

#include "editor_gui_common_includes.h"
#include "engine/editor/editor_layout.h"
#include "engine/editor/project_manager.h"
#include "engine/input/input_action_system.h"
#include "engine/input/input_actions.h"

#include <string>
#include <vector>

namespace {

using input_actions::ActionDef;
using input_actions::ActionType;
using input_actions::CapturePart;

std::vector<ActionDef> g_defs;
bool g_loaded = false;
std::string g_loadedRoot;  // project the page was read from
std::string g_loadError;
std::string g_status;
std::vector<input_actions::Issue> g_issues;

// Listen (capture) state for one binding.
input_actions::Capture g_listen;
int g_listenAction = -1;
int g_listenBinding = -1;
CapturePart g_listenPart = CapturePart::Whole;
bool g_openListenPopup = false;

// Delete confirmation.
int g_deleteAction = -1;
bool g_openDeletePopup = false;

const ImVec4 kErrorColor(1.0f, 0.4f, 0.4f, 1.0f);

void Load() {
    g_defs.clear();
    g_issues.clear();
    g_loadError.clear();
    g_status.clear();
    std::string text;
    if (!ProjectManager::Instance().LoadInputActionsJson(&text, &g_loadError)) {
        g_loaded = true;
        return;
    }
    if (!text.empty()) {
        std::vector<input_actions::Issue> parseIssues;
        g_defs = input_actions::ParseActions(nlohmann::json::parse(text, nullptr, false), &parseIssues);
        if (!parseIssues.empty())
            g_status = "Some entries in project.json could not be read and are not shown; saving will drop them.";
    }
    g_issues = input_actions::Validate(g_defs);
    g_loaded = true;
}

// Writes the working set when it has no problems; otherwise keeps it in the page.
void Save() {
    g_issues = input_actions::Validate(g_defs);
    if (!g_issues.empty()) {
        g_status = "Not saved: fix the problems below first.";
        return;
    }
    std::string error;
    if (!ProjectManager::Instance().SaveInputActionsJson(input_actions::ActionsToJson(g_defs).dump(), &error)) {
        g_status = "Not saved: " + error;
        return;
    }
    InputActionSystem_ReloadDefaults();
    g_status = "Saved to project.json.";
}

std::string UniqueName(const char* base) {
    for (int n = 1;; ++n) {
        const std::string candidate = n == 1 ? std::string(base) : std::string(base) + std::to_string(n);
        bool used = false;
        for (const ActionDef& d : g_defs)
            used = used || d.name == candidate;
        if (!used)
            return candidate;
    }
}

void StartListen(int action, int binding, CapturePart part) {
    input_actions::Snapshot now;
    {
        std::lock_guard<std::mutex> lock(InputActionSystem_Mutex());
        now = InputActionSystem_LastSnapshot();
    }
    // Mouse excluded: clicking Cancel must not become a binding.
    g_listen.Start(now, false);
    g_listenAction = action;
    g_listenBinding = binding;
    g_listenPart = part;
    g_openListenPopup = true;
}

void DrawListenPopup() {
    if (g_openListenPopup) {
        ImGui::OpenPopup("Listen for input");
        g_openListenPopup = false;
    }
    if (!ImGui::BeginPopupModal("Listen for input", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    input_actions::Snapshot now;
    {
        std::lock_guard<std::mutex> lock(InputActionSystem_Mutex());
        now = InputActionSystem_LastSnapshot();
    }
    const auto state = g_listen.Update(now);
    const bool valid = g_listenAction >= 0 && g_listenAction < static_cast<int>(g_defs.size());
    if (state == input_actions::Capture::State::Done && valid) {
        ActionDef& def = g_defs[g_listenAction];
        const bool replacing = g_listenBinding < static_cast<int>(def.bindings.size());
        const std::string binding = input_actions::ComposeCapturedBinding(
            def.type, g_listenPart, replacing ? std::string_view(def.bindings[g_listenBinding]) : "",
            g_listen.Result());
        if (replacing)
            def.bindings[g_listenBinding] = binding;
        else
            def.bindings.push_back(binding);
        Save();
        ImGui::CloseCurrentPopup();
    } else if (state != input_actions::Capture::State::Listening || !valid) {
        ImGui::CloseCurrentPopup();
    } else {
        const char* what = g_listenPart == CapturePart::Positive ? "positive side"
                         : g_listenPart == CapturePart::Negative ? "negative side"
                                                                 : "binding";
        ImGui::Text("Action '%s': press a key, pad button, trigger or stick for the %s.",
                    g_defs[g_listenAction].name.c_str(), what);
        ImGui::TextDisabled("Esc or Cancel stops. Mouse buttons: type Mouse.Left / .Right / .Middle instead.");
        if (ImGui::Button("Cancel")) {
            g_listen.Cancel();
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::EndPopup();
}

void DrawDeletePopup() {
    if (g_openDeletePopup) {
        ImGui::OpenPopup("Delete input action");
        g_openDeletePopup = false;
    }
    if (!ImGui::BeginPopupModal("Delete input action", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    const bool valid = g_deleteAction >= 0 && g_deleteAction < static_cast<int>(g_defs.size());
    if (valid) {
        ImGui::Text("Delete action '%s'? Scripts that read it will get false / 0.", g_defs[g_deleteAction].name.c_str());
        if (ImGui::Button("Delete")) {
            g_defs.erase(g_defs.begin() + g_deleteAction);
            Save();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
    }
    if (ImGui::Button("Keep") || !valid)
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void DrawProblems(const std::string& action) {
    for (const input_actions::Issue& issue : g_issues) {
        if (issue.action == action)
            ImGui::TextColored(kErrorColor, "%s", issue.message.c_str());
    }
}

void DrawAction(int index) {
    ActionDef& def = g_defs[index];
    ImGui::PushID(index);
    const std::string header = def.name + "  (" + input_actions::ActionTypeName(def.type) + ")###action";
    if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        char nameBuf[128];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", def.name.c_str());
        ImGui::SetNextItemWidth(Ui(200.0f));
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
            def.name = nameBuf;
        if (ImGui::IsItemDeactivatedAfterEdit())
            Save();

        int type = def.type == ActionType::Axis ? 1 : 0;
        ImGui::SetNextItemWidth(Ui(200.0f));
        if (ImGui::Combo("Type", &type, "Button (pressed / released)\0Axis (-1 .. 1)\0")) {
            def.type = type == 1 ? ActionType::Axis : ActionType::Button;
            Save();
        }

        int removeBinding = -1;
        for (int b = 0; b < static_cast<int>(def.bindings.size()); ++b) {
            ImGui::PushID(b);
            char buf[128];
            std::snprintf(buf, sizeof(buf), "%s", def.bindings[b].c_str());
            ImGui::SetNextItemWidth(Ui(240.0f));
            if (ImGui::InputText("##binding", buf, sizeof(buf)))
                def.bindings[b] = buf;
            if (ImGui::IsItemDeactivatedAfterEdit())
                Save();
            ImGui::SameLine();
            if (ImGui::SmallButton("Listen"))
                StartListen(index, b, CapturePart::Whole);
            if (def.type == ActionType::Axis) {
                ImGui::SameLine();
                if (ImGui::SmallButton("+"))
                    StartListen(index, b, CapturePart::Positive);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Listen for the positive side (A in A/B)");
                ImGui::SameLine();
                if (ImGui::SmallButton("-"))
                    StartListen(index, b, CapturePart::Negative);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Listen for the negative side (B in A/B)");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X"))
                removeBinding = b;
            ImGui::PopID();
        }
        if (removeBinding >= 0) {
            def.bindings.erase(def.bindings.begin() + removeBinding);
            Save();
        }
        if (ImGui::Button("Add binding (Listen)"))
            StartListen(index, static_cast<int>(def.bindings.size()), CapturePart::Whole);
        ImGui::SameLine();
        if (ImGui::Button("Delete action")) {
            g_deleteAction = index;
            g_openDeletePopup = true;
        }
        DrawProblems(def.name);
        ImGui::Spacing();
    }
    ImGui::PopID();
}

}  // namespace

void DrawProjectInputSettings() {
    ImGui::Text("Input Actions");
    ImGui::TextDisabled("Stored in project.json (input.actions). Scripts read them with ScriptApi_Input*.");
    ImGui::TextDisabled("Bindings: Key.SPACE, Pad.A, Pad.LT, Pad.LeftStickX, Pad.LeftStickY+, Mouse.Left, Key.D/Key.A (axis).");
    ImGui::Separator();

    // Read on first view and whenever another project was opened since.
    if (!g_loaded || g_loadedRoot != ProjectManager::Instance().GetRootPath()) {
        Load();
        g_loadedRoot = ProjectManager::Instance().GetRootPath();
    }
    if (ImGui::Button("Reload from project.json"))
        Load();
    ImGui::SameLine();
    const bool canEdit = g_loadError.empty();
    if (!canEdit)
        ImGui::BeginDisabled();
    if (ImGui::Button("Add action")) {
        g_defs.push_back({UniqueName("NewAction"), ActionType::Button, {}});
        Save();
    }
    if (!canEdit)
        ImGui::EndDisabled();

    if (!g_loadError.empty())
        ImGui::TextColored(kErrorColor, "%s", g_loadError.c_str());
    if (!g_status.empty())
        ImGui::TextWrapped("%s", g_status.c_str());
    DrawProblems("");
    ImGui::Separator();

    if (canEdit) {
        if (g_defs.empty())
            ImGui::TextDisabled("No actions yet. Add one, then Listen for a key or pad input.");
        for (int i = 0; i < static_cast<int>(g_defs.size()); ++i)
            DrawAction(i);
    }
    DrawListenPopup();
    DrawDeletePopup();
}
