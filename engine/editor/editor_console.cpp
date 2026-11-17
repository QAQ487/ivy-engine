// editor_console.cpp
// Dockable Console panel: ring-buffer log viewer with filters / search / copy.

#include "editor_gui_common_includes.h"
#include "engine/editor/editor_console.h"
#include "engine/debug_draw/debug_ofstream.h"

#include <chrono>
#include <cstring>

namespace {

EditorConsoleBuffer g_buffer(2000);
EditorConsoleFilter g_filter{};
bool g_clearOnPlay = false;
bool g_autoScrollEnabled = true;  // user checkbox
bool g_autoScrollPaused = false;  // true while scrolled away from bottom
std::uint64_t g_selectedId = 0;   // 0 = none

std::int64_t NowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void LogHook(const char* text, void* /*user*/)
{
    if (!text || !*text)
        return;
    g_buffer.PushAutoLevel(text, NowMs());
}

ImVec4 LevelColor(EditorConsoleLevel level)
{
    const auto& sem = editor_theme::GetSemanticColors();
    switch (level) {
    case EditorConsoleLevel::Warning:
        return sem.warning.w > 0.0f ? sem.warning : ImVec4(0.95f, 0.75f, 0.20f, 1.0f);
    case EditorConsoleLevel::Error:
        return sem.danger.w > 0.0f ? sem.danger : ImVec4(0.95f, 0.35f, 0.30f, 1.0f);
    default:
        return sem.text.w > 0.0f ? sem.text : ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
    }
}

int IndexOfSelected(const std::vector<EditorConsoleEntry>& entries)
{
    if (g_selectedId == 0)
        return -1;
    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        if (entries[static_cast<size_t>(i)].id == g_selectedId)
            return i;
    }
    return -1;
}

void CopyEntriesToClipboard(const std::vector<EditorConsoleEntry>& entries)
{
    std::string blob;
    const int selected = IndexOfSelected(entries);
    if (selected >= 0) {
        blob = FormatConsoleEntryLine(entries[static_cast<size_t>(selected)]);
    }
    else {
        blob.reserve(entries.size() * 64);
        for (size_t i = 0; i < entries.size(); ++i) {
            if (i) blob.push_back('\n');
            blob.append(FormatConsoleEntryLine(entries[i]));
        }
    }
    ImGui::SetClipboardText(blob.c_str());
}

void DockConsoleFirstUseEver()
{
    // Existing projects' imgui_layout.ini has no Console entry — dock into the
    // Asset Browser node on first use so it does not open floating.
    if (ImGuiWindow* asset = ImGui::FindWindowByName(kEditorAssetBrowserWindowTitle)) {
        if (asset->DockId != 0)
            ImGui::SetNextWindowDockID(asset->DockId, ImGuiCond_FirstUseEver);
    }
    ImGui::SetNextWindowSize(ImVec2(Ui(640.0f), Ui(280.0f)), ImGuiCond_FirstUseEver);
}

} // namespace

EditorConsoleBuffer& EditorConsole_Buffer() { return g_buffer; }
EditorConsoleFilter& EditorConsole_Filter() { return g_filter; }
bool& EditorConsole_ClearOnPlay() { return g_clearOnPlay; }

void EditorConsole_InstallLogHook()
{
    hal::SetDebugLogHook(&LogHook, nullptr);
}

void EditorConsole_RemoveLogHook()
{
    hal::ClearDebugLogHook();
}

void EditorConsole_Clear()
{
    g_buffer.Clear();
    g_selectedId = 0;
}

void EditorConsole_OnPlayStarted()
{
    if (g_clearOnPlay)
        EditorConsole_Clear();
}

void EditorConsole_WritePrefs(nlohmann::json& j)
{
    if (!j.is_object())
        j = nlohmann::json::object();
    j["consoleShowInfo"] = g_filter.showInfo;
    j["consoleShowWarning"] = g_filter.showWarning;
    j["consoleShowError"] = g_filter.showError;
    j["consoleClearOnPlay"] = g_clearOnPlay;
}

void EditorConsole_LoadPrefs(const nlohmann::json& j)
{
    if (!j.is_object())
        return;
    if (j.contains("consoleShowInfo") && j["consoleShowInfo"].is_boolean())
        g_filter.showInfo = j["consoleShowInfo"].get<bool>();
    if (j.contains("consoleShowWarning") && j["consoleShowWarning"].is_boolean())
        g_filter.showWarning = j["consoleShowWarning"].get<bool>();
    if (j.contains("consoleShowError") && j["consoleShowError"].is_boolean())
        g_filter.showError = j["consoleShowError"].get<bool>();
    if (j.contains("consoleClearOnPlay") && j["consoleClearOnPlay"].is_boolean())
        g_clearOnPlay = j["consoleClearOnPlay"].get<bool>();
}

void DrawEditorConsolePanel(bool* open)
{
    if (open && !*open)
        return;

    DockConsoleFirstUseEver();
    if (!ImGui::Begin(kEditorConsoleWindowTitle, open)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear"))
        EditorConsole_Clear();
    ImGui::SameLine();
    const auto entriesPtr = g_buffer.Snapshot(g_filter);
    static const std::vector<EditorConsoleEntry> kEmptyEntries;
    const std::vector<EditorConsoleEntry>& entries =
        entriesPtr ? *entriesPtr : kEmptyEntries;
    const int selectedIndex = IndexOfSelected(entries);
    if (selectedIndex < 0)
        g_selectedId = 0;
    if (ImGui::Button(selectedIndex >= 0 ? "Copy Selected" : "Copy All"))
        CopyEntriesToClipboard(entries);
    ImGui::SameLine();
    if (ImGui::Checkbox("Auto-scroll", &g_autoScrollEnabled)) {
        if (g_autoScrollEnabled)
            g_autoScrollPaused = false;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Clear on Play", &g_clearOnPlay))
        SaveUiPrefs();

    ImGui::Separator();
    bool filterChanged = false;
    filterChanged |= ImGui::Checkbox("Info", &g_filter.showInfo);
    ImGui::SameLine();
    filterChanged |= ImGui::Checkbox("Warning", &g_filter.showWarning);
    ImGui::SameLine();
    filterChanged |= ImGui::Checkbox("Error", &g_filter.showError);
    if (filterChanged)
        SaveUiPrefs();

    ImGui::SameLine();
    ImGui::SetNextItemWidth(Ui(220.0f));
    char searchBuf[256];
    std::snprintf(searchBuf, sizeof(searchBuf), "%s", g_filter.search.c_str());
    if (ImGui::InputTextWithHint("##ConsoleSearch", "Search...", searchBuf, sizeof(searchBuf)))
        g_filter.search = searchBuf;

    ImGui::Separator();

    const float footer = ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("##ConsoleLog", ImVec2(0.0f, -footer), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(entries.size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const EditorConsoleEntry& e = entries[static_cast<size_t>(i)];
                const std::string line = FormatConsoleEntryLine(e);
                ImGui::PushID(static_cast<int>(e.id));
                ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(e.level));
                const bool selected = (e.id == g_selectedId);
                if (ImGui::Selectable(line.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    g_selectedId = e.id;
                    if (ImGui::IsMouseDoubleClicked(0))
                        ImGui::SetClipboardText(line.c_str());
                }
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
        }

        if (ImGui::GetScrollMaxY() > 0.0f) {
            const float fromBottom = ImGui::GetScrollMaxY() - ImGui::GetScrollY();
            if (fromBottom > Ui(24.0f))
                g_autoScrollPaused = true;
            else if (fromBottom <= 1.0f)
                g_autoScrollPaused = false;
        }
        if (g_autoScrollEnabled && !g_autoScrollPaused)
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    ImGui::TextDisabled("%zu lines", entries.size());
    ImGui::End();
}
