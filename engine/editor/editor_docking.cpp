// editor_docking.cpp
// DockSpace host with PassthruCentralNode; default DockBuilder layout; per-project ini.

#include "editor_gui_common_includes.h"
#include "engine/editor/editor_docking.h"

#include <fstream>

namespace {

constexpr const char* kDockspaceWindowName = "##EditorDockHost";

EditorPanelVisibility g_panelVisibility{};
EditorClientRect g_centralClient{};
bool g_hasCentral = false;
bool g_resetLayoutRequested = false;
bool g_forceDefaultLayout = false;
std::string g_activeIniPathUtf8;
std::string g_pendingOpenRootUtf8;
bool g_pendingClose = false;

bool ReadFileUtf8(const std::filesystem::path& path, std::string& out)
{
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec)
        return false;
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

bool WriteFileUtf8(const std::filesystem::path& path, const char* data, size_t size)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    out.write(data, static_cast<std::streamsize>(size));
    return static_cast<bool>(out);
}

void SaveIniToActivePath()
{
    if (g_activeIniPathUtf8.empty() || !g_imguiInitialized)
        return;
    size_t size = 0;
    const char* data = ImGui::SaveIniSettingsToMemory(&size);
    if (!data)
        return;
    WriteFileUtf8(PathFromUtf8(g_activeIniPathUtf8), data, size);
    ImGui::GetIO().WantSaveIniSettings = false;
}

void ApplyDefaultDockLayout(ImGuiID dockspaceId, ImVec2 hostSize, float uiZoom)
{
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, hostSize);

    const EditorDockSplitRatios ratios =
        ComputeDefaultDockSplitRatios(hostSize.x, hostSize.y, uiZoom);

    ImGuiID dockMain = dockspaceId;
    ImGuiID dockLeft = 0;
    ImGuiID dockRight = 0;
    ImGuiID dockBottom = 0;
    dockMain = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Left, ratios.left, &dockLeft, &dockMain);
    dockMain = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Right, ratios.right, &dockRight, &dockMain);
    dockMain = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Down, ratios.bottom, &dockBottom, &dockMain);

    ImGui::DockBuilderDockWindow(kEditorHierarchyWindowTitle, dockLeft);
    ImGui::DockBuilderDockWindow(kEditorPropertiesWindowTitle, dockRight);
    ImGui::DockBuilderDockWindow(kEditorAssetBrowserWindowTitle, dockBottom);
    // Tab with Asset Browser at the bottom (same dock node).
    ImGui::DockBuilderDockWindow(kEditorConsoleWindowTitle, dockBottom);
    ImGui::DockBuilderFinish(dockspaceId);

    g_panelVisibility.hierarchy = true;
    g_panelVisibility.properties = true;
    g_panelVisibility.assetBrowser = true;
    g_panelVisibility.console = true;
}

void CaptureCentralNode(ImGuiID dockspaceId)
{
    g_hasCentral = false;
    g_centralClient = {};
    if (ImGuiDockNode* node = ImGui::DockBuilderGetCentralNode(dockspaceId)) {
        g_centralClient.x = node->Pos.x;
        g_centralClient.y = node->Pos.y;
        g_centralClient.w = node->Size.x;
        g_centralClient.h = node->Size.y;
        g_hasCentral = IsUsableViewportRect(g_centralClient);
        if (!g_hasCentral && node->Size.x > 1.0f && node->Size.y > 1.0f) {
            // Allow slightly small rects (resize mid-drag) without treating as missing.
            g_hasCentral = node->Size.x >= 1.0f && node->Size.y >= 1.0f;
            if (g_centralClient.w < 1.0f) g_centralClient.w = 1.0f;
            if (g_centralClient.h < 1.0f) g_centralClient.h = 1.0f;
        }
    }
}

} // namespace

void EditorDocking_ConfigureImGuiIO()
{
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // Multi-viewports intentionally off (Phase 2).
    io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
    io.IniFilename = nullptr;
}

void EditorDocking_OnProjectOpened(const std::string& projectRootUtf8)
{
    // May be called mid-frame (folder pick / unsaved dialog). Defer Clear/Load
    // until EditorDocking_BeginFrame (after NewFrame).
    g_pendingClose = false;
    g_pendingOpenRootUtf8 = projectRootUtf8;
}

void EditorDocking_OnProjectClosed()
{
    g_pendingOpenRootUtf8.clear();
    g_pendingClose = true;
}

void EditorDocking_BeginFrame()
{
    if (!g_imguiInitialized)
        return;

    if (g_pendingClose) {
        SaveIniToActivePath();
        g_activeIniPathUtf8.clear();
        g_forceDefaultLayout = false;
        g_resetLayoutRequested = false;
        g_hasCentral = false;
        g_centralClient = {};
        ImGui::ClearIniSettings();
        g_pendingClose = false;
    }

    if (!g_pendingOpenRootUtf8.empty()) {
        if (!g_activeIniPathUtf8.empty())
            SaveIniToActivePath();

        g_activeIniPathUtf8 = ResolveEditorImGuiLayoutIniPath(g_pendingOpenRootUtf8);
        g_forceDefaultLayout = true;
        g_resetLayoutRequested = false;
        g_hasCentral = false;
        g_centralClient = {};

        ImGui::ClearIniSettings();
        if (!g_activeIniPathUtf8.empty()) {
            std::string data;
            if (ReadFileUtf8(PathFromUtf8(g_activeIniPathUtf8), data) && !data.empty()) {
                ImGui::LoadIniSettingsFromMemory(data.c_str(), data.size());
                g_forceDefaultLayout = false;
            }
        }
        g_pendingOpenRootUtf8.clear();
    }
}

void EditorDocking_FlushIniIfNeeded()
{
    if (!g_imguiInitialized || g_activeIniPathUtf8.empty())
        return;
    if (ImGui::GetIO().WantSaveIniSettings)
        SaveIniToActivePath();
}

void EditorDocking_RequestResetLayout()
{
    g_resetLayoutRequested = true;
}

void EditorDocking_DrawHost()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float topChrome = EditorTopChrome();
    ImVec2 hostPos(viewport->Pos.x, viewport->Pos.y + topChrome);
    ImVec2 hostSize(viewport->Size.x, viewport->Size.y - topChrome);
    if (hostSize.x < 1.0f) hostSize.x = 1.0f;
    if (hostSize.y < 1.0f) hostSize.y = 1.0f;

    ImGui::SetNextWindowPos(hostPos);
    ImGui::SetNextWindowSize(hostSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin(kDockspaceWindowName, nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    const ImGuiID dockspaceId = ImGui::GetID("EditorDockSpace");
    const ImGuiDockNodeFlags dockFlags =
        ImGuiDockNodeFlags_PassthruCentralNode |
        ImGuiDockNodeFlags_NoDockingOverCentralNode;

    // Do not key off DockBuilderGetNode==null: after LoadIniSettings the node
    // may be absent until DockSpace runs; rebuilding would wipe saved layout.
    if (g_resetLayoutRequested || g_forceDefaultLayout) {
        ApplyDefaultDockLayout(dockspaceId, hostSize, g_uiZoom);
        g_resetLayoutRequested = false;
        g_forceDefaultLayout = false;
    }

    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockFlags);
    CaptureCentralNode(dockspaceId);
    ImGui::End();

    // Fallback capture if central node missing (first frame / minimized).
    if (!g_hasCentral) {
        g_centralClient.x = hostPos.x;
        g_centralClient.y = hostPos.y;
        g_centralClient.w = (std::max)(kEditorDockViewportMinWidth, hostSize.x);
        g_centralClient.h = (std::max)(kEditorDockViewportMinHeight, hostSize.y);
        g_hasCentral = true;
    }
}

const EditorClientRect& EditorDocking_GetCentralNodeClientRect()
{
    return g_centralClient;
}

bool EditorDocking_HasCentralNode()
{
    return g_hasCentral;
}

EditorClientRect EditorDocking_GetGameViewportClientRect()
{
    if (g_hasCentral)
        return FitEditor16x9Viewport(g_centralClient);
    return FitEditor16x9Viewport(EditorClientRect{0.0f, 0.0f, 1.0f, 1.0f});
}

void EditorDocking_DrawLetterboxBars()
{
    if (!g_hasCentral)
        return;

    const EditorClientRect& node = g_centralClient;
    const EditorClientRect game = FitEditor16x9Viewport(node);
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImU32 black = IM_COL32(0, 0, 0, 255);

    // Top / bottom letterbox strips (full node width).
    if (game.y > node.y + 0.5f)
        dl->AddRectFilled(
            ImVec2(node.x, node.y), ImVec2(node.x + node.w, game.y), black);
    const float gameBottom = game.y + game.h;
    const float nodeBottom = node.y + node.h;
    if (gameBottom + 0.5f < nodeBottom)
        dl->AddRectFilled(
            ImVec2(node.x, gameBottom), ImVec2(node.x + node.w, nodeBottom), black);

    // Left / right pillarbox strips (only beside the game rect).
    if (game.x > node.x + 0.5f)
        dl->AddRectFilled(
            ImVec2(node.x, game.y), ImVec2(game.x, gameBottom), black);
    const float gameRight = game.x + game.w;
    const float nodeRight = node.x + node.w;
    if (gameRight + 0.5f < nodeRight)
        dl->AddRectFilled(
            ImVec2(gameRight, game.y), ImVec2(nodeRight, gameBottom), black);
}

EditorPanelVisibility& EditorDocking_PanelVisibility()
{
    return g_panelVisibility;
}

bool EditorDocking_WantCaptureMouseForGame()
{
    // With PassthruCentralNode, WantCaptureMouse is false over the empty central
    // node (scene shows through) and true over docked panels / tab bars / menus.
    return ImGui::GetIO().WantCaptureMouse;
}
