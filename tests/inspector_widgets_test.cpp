// inspector_widgets_test.cpp
// Headless Dear ImGui check: every inspector widget stays inside the panel
// width (no horizontal overflow) from very narrow to wide panels.
// Built and run by tools/run_inspector_widgets_check.sh.
#include "engine/editor/inspector_widgets.h"
#include "engine/ecs/component.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/icons_font_awesome.h"

#include <cstdio>
#include <string>

namespace {
struct FakeComponent : Component {
    const char* GetTypeName() const override { return "Fake"; }
};
int g_fail = 0;

void DrawAll()
{
    FakeComponent comp;
    static bool enabled = true, a = true, b = false, c = true;
    static float v3[3] = { 1.0f, 2.0f, 3.0f };
    static float f = 0.5f;
    static int choice = 1;
    static std::string path = "rom/texture/a_very_long_texture_name_that_never_fits.png";
    const char* exts[] = { ".png" };

    insp::HeaderResult h = insp::ComponentHeader("Fake", ICON_FA_CUBE,
        "AVeryLongComponentNameThatDoesNotFit", insp::Accent::Render, &enabled, &comp);
    (void)h;
    insp::DragVecN(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, "Position", "Local", v3, 3, 0.1f);
    insp::DragVecN(nullptr, "Size", nullptr, v3, 2, 0.1f);
    { insp::PropRow row(ICON_FA_WEIGHT_HANGING, "ExtremelyLongPropertyLabel", "tip");
      ImGui::DragFloat("##v", &f); }
    { insp::PropRow row(nullptr, "Mode"); ImGui::Combo("##v", &choice, "A\0B\0C\0"); }
    insp::Toggle toggles[] = { { ICON_FA_REPEAT, "Loop", &a }, { ICON_FA_PLAY, "Autoplay", &b },
                               { ICON_FA_CUBE, "Spatial", &c }, { ICON_FA_EYE, "Debug", &a } };
    insp::ToggleRow(toggles, 4);
    const char* icons[] = { ICON_FA_MUSIC, ICON_FA_BOLT, ICON_FA_MICROPHONE };
    const char* tips[] = { "BGM", "SFX", "Voice" };
    insp::ChoiceBar("bus", icons, tips, 3, &choice);
    insp::IconButtonRow("ord", icons, tips, 3);
    insp::AssetField(ICON_FA_IMAGE, "Texture", nullptr, path, exts, 1);
    insp::WideButton(ICON_FA_PLUS, "Add");
    insp::SubHeading(ICON_FA_LAYER_GROUP, "LOD");
    ImGui::TextUnformatted(ICON_FA_CUBE);
    insp::HintIconSameLine(ICON_FA_TRIANGLE_EXCLAMATION, insp::WarningColor(), "warn");
}

void RunAtWidth(float width, float fontScale)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::GetStyle().FontScaleMain = fontScale;
    for (int frame = 0; frame < 3; ++frame) {
        io.DisplaySize = ImVec2(2000.0f, 2000.0f);
        io.DeltaTime = 1.0f / 60.0f;
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(width, 1900.0f));
        ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoSavedSettings);
        DrawAll();
        ImGuiWindow* w = ImGui::GetCurrentWindow();
        // Framed headers legitimately reach half the window padding past the
        // content edge (Dear ImGui CollapsingHeader behaviour; no scrollbar).
        const float allowed = ImGui::GetStyle().WindowPadding.x * 0.5f;
        const float overflow = w->DC.CursorMaxPos.x - w->WorkRect.Max.x - allowed;
        if (frame == 2 && overflow > 0.5f) {
            std::printf("FAIL width %.0f scale %.1f: content overflows by %.1f px\n",
                width, fontScale, overflow);
            ++g_fail;
        }
        ImGui::End();
        ImGui::Render();
    }
}
} // namespace

int main()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int fw = 0, fh = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &fw, &fh);
    io.Fonts->SetTexID((ImTextureID)1);

    const float widths[] = { 120.0f, 160.0f, 200.0f, 240.0f, 320.0f, 480.0f, 900.0f };
    const float scales[] = { 1.0f, 1.5f, 2.0f };
    for (float s : scales)
        for (float w : widths)
            RunAtWidth(w, s);

    ImGui::DestroyContext();
    if (g_fail == 0)
        std::printf("inspector_widgets_test: all passed\n");
    return g_fail == 0 ? 0 : 1;
}
