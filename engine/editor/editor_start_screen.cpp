// editor_start_screen.cpp
// Extracted from engine/core/engine_gui.cpp (phase-1 editor UI split; behaviour-preserving).

#include "editor_gui_common_includes.h"

static ID3D11ShaderResourceView* GetIvyLogoSrv() {
    static ID3D11ShaderResourceView* s_srv = nullptr;
    static bool s_attempted = false;
    if (!s_attempted) {
        s_attempted = true;
        ID3D11Device* device = GetDevice();
        if (device) {
            ID3D11Resource* tex = nullptr;
            const std::wstring logo = content_path::ReadableWideCurrent(L"rom/branding/ivy_logo.png");
            if (FAILED(DirectX::CreateWICTextureFromFile(
                    device, logo.empty() ? L"rom/branding/ivy_logo.png" : logo.c_str(), &tex, &s_srv))) {
                s_srv = nullptr;
            }
            if (tex)
                tex->Release();
        }
    }
    return s_srv;
}

void DrawEngineStartScreen() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    const auto& theme = editor_theme::GetSemanticColors();
    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme.windowBg);
    ImGui::Begin("##EngineStartScreen", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
    ImGui::PopStyleColor();

    const ImVec2 cardSize(480.0f, 520.0f);
    const ImVec2 cardPos(
        viewport->Pos.x + (viewport->Size.x - cardSize.x) * 0.5f,
        viewport->Pos.y + (viewport->Size.y - cardSize.y) * 0.5f);
    ImGui::SetCursorScreenPos(cardPos);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Ui(36.0f), Ui(30.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text);
    ImGui::BeginChild("##StartCard", cardSize, ImGuiChildFlags_Borders);

    if (ID3D11ShaderResourceView* logoSrv = GetIvyLogoSrv()) {
        const float logoSize = 160.0f;
        const float avail = ImGui::GetContentRegionAvail().x;
        if (logoSize < avail)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - logoSize) * 0.5f);
        ImGui::Image(ImTextureRef((ImTextureID)(intptr_t)logoSrv), ImVec2(logoSize, logoSize));
        ImGui::Dummy(ImVec2(0.0f, 12.0f));
    }

    {
        const char* title = "Ivy Engine";
        const float titleW = ImGui::CalcTextSize(title).x;
        const float avail = ImGui::GetContentRegionAvail().x;
        if (titleW < avail)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - titleW) * 0.5f);
        ImGui::TextUnformatted(title);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, theme.textMuted);
    {
        const char* subtitle = "Custom D3D11 game engine";
        const float subW = ImGui::CalcTextSize(subtitle).x;
        const float avail = ImGui::GetContentRegionAvail().x;
        if (subW < avail)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - subW) * 0.5f);
        ImGui::TextUnformatted(subtitle);
    }
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    const float btnW = ImGui::GetContentRegionAvail().x;
    const float btnH = 44.0f;
    ImGui::PushStyleColor(ImGuiCol_Button, theme.success);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme.successHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme.successActive);
    if (ImGui::Button("Open Project...", ImVec2(btnW, btnH)))
        g_pendingFolderPick = PendingFolderPickAction::OpenProject;
    ImGui::PopStyleColor(3);
    if (!ProjectManager::Instance().ResolveInRepoTestbedPath().empty()) {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        if (ImGui::Button("Open Testbed", ImVec2(btnW, btnH)))
            RequestOpenTestbed();
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    if (ImGui::Button("New Project...", ImVec2(btnW, btnH)))
        g_pendingFolderPick = PendingFolderPickAction::NewProject;

    ImGui::Dummy(ImVec2(0.0f, 18.0f));
    ImGui::TextUnformatted("Recent projects");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    const auto& recent = ProjectManager::Instance().GetRecentProjects();
    if (recent.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.textMuted);
        ImGui::TextUnformatted("(No recent projects)");
        ImGui::PopStyleColor();
    }
    else {
        const size_t count = (std::min)(recent.size(), size_t{5});
        for (size_t i = 0; i < count; ++i) {
            const std::string& path = recent[i];
            const std::string leaf = PathToUtf8(PathFromUtf8(path).filename());
            const std::string label = leaf.empty() ? path : leaf;
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::SmallButton(label.c_str()))
                RequestOpenProject(path);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", path.c_str());
            ImGui::PopID();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    ImGui::End();
}

