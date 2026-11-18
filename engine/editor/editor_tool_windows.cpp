// editor_tool_windows.cpp
// Extracted from engine/core/engine_gui.cpp (phase-1 editor UI split; behaviour-preserving).

#include "editor_gui_common_includes.h"
#include "engine/scene/scene_lighting.h"
#include "engine/rendering/culling/fx_cull.h"
#include "engine/rendering/culling/cull_settings_math.h"
#include "engine/rendering/culling/lod_select_math.h"
#include "engine/rendering/culling/lod_state.h"
#include "engine/rendering/lighting/lighting.h"

std::string ShaderVariantPreviewLabel(Shader3DVariant v) {
    const char* label = Shader3DVariantLabel(v);
    if (Shader3DVariantIsUserVisible(v))
        return label;
    return std::string(label) + " (legacy)";
}

void DrawPostFxShowcaseWindow(bool* open) {
    if (!open || !*open) return;

    ImGui::SetNextWindowSize(ImVec2(Ui(380.0f), 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("PostFX Showcase", open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    static PostFXParams s_fx;
    static bool s_synced = false;
    if (!s_synced) {
        PostProcess_GetEffects(s_fx);
        s_synced = true;
    }

    auto applyFx = [&]() { PostProcess_SetEffects(s_fx); };

    ImGui::TextWrapped(
        "Live stackable post-process over the game viewport. "
        "Strength 0 = off for each effect.");
    ImGui::Separator();

    if (ImGui::SliderFloat("RGB Shift", &s_fx.rgbShift, 0.0f, 0.04f, "%.4f"))
        applyFx();
    if (ImGui::SliderFloat("Mosaic Blocks", &s_fx.mosaicBlocks, 0.0f, 80.0f, "%.0f"))
        applyFx();
    if (ImGui::SliderFloat("Posterize", &s_fx.posterize, 0.0f, 32.0f, "%.0f"))
        applyFx();
    if (ImGui::SliderFloat("Noise", &s_fx.noise, 0.0f, 1.0f, "%.3f"))
        applyFx();
    if (ImGui::SliderFloat("Grayscale", &s_fx.grayscale, 0.0f, 1.0f, "%.3f"))
        applyFx();
    if (ImGui::SliderFloat("Contrast (1.0 = neutral)", &s_fx.contrast, 0.0f, 2.0f, "%.2f"))
        applyFx();
    if (ImGui::SliderFloat("Vignette", &s_fx.vignette, 0.0f, 1.0f, "%.3f"))
        applyFx();
    if (ImGui::SliderFloat("Scanline", &s_fx.scanline, 0.0f, 1.0f, "%.3f"))
        applyFx();
    if (ImGui::SliderFloat("Flicker", &s_fx.flicker, 0.0f, 1.0f, "%.3f"))
        applyFx();

    ImGui::Separator();
    ImGui::Text("Bloom (Bright Glow)");
    if (ImGui::SliderFloat("Bloom Intensity", &s_fx.bloomIntensity, 0.0f, 2.5f, "%.2f"))
        applyFx();
    if (ImGui::SliderFloat("Bloom Threshold", &s_fx.bloomThreshold, 0.0f, 1.5f, "%.2f"))
        applyFx();
    if (ImGui::SliderFloat("Bloom Spread", &s_fx.bloomSpread, 0.1f, 4.0f, "%.2f"))
        applyFx();

    ImGui::Spacing();
    if (ImGui::Button("Off")) {
        s_fx = PostFXParams{};
        applyFx();
    }
    ImGui::SameLine();
    if (ImGui::Button("Bloom Only")) {
        s_fx = PostFXParams{};
        s_fx.bloomIntensity = 0.8f;
        s_fx.bloomThreshold = 0.7f;
        s_fx.bloomSpread = 1.2f;
        applyFx();
    }
    ImGui::SameLine();
    if (ImGui::Button("Dreamy Bloom")) {
        s_fx = PostFXParams{};
        s_fx.bloomIntensity = 1.5f;
        s_fx.bloomThreshold = 0.5f;
        s_fx.bloomSpread = 2.2f;
        s_fx.contrast = 1.1f;
        applyFx();
    }

    ImGui::Spacing();
    if (ImGui::Button("Backrooms VHS")) {
        s_fx = PostFXParams{};
        s_fx.noise = 0.12f;
        s_fx.vignette = 0.55f;
        s_fx.scanline = 0.25f;
        s_fx.contrast = 1.15f;
        s_fx.rgbShift = 0.0015f;
        s_fx.flicker = 0.08f;
        applyFx();
    }
    ImGui::SameLine();
    if (ImGui::Button("Emo")) {
        s_fx = PostFXParams{};
        s_fx.grayscale = 1.0f;
        s_fx.contrast = 1.4f;
        s_fx.noise = 0.15f;
        s_fx.posterize = 8.0f;
        applyFx();
    }

    ImGui::Spacing();
    if (ImGui::Button("Preset: Mild RGB")) {
        s_fx = PostFXParams{};
        s_fx.rgbShift = 0.008f;
        applyFx();
    }
    ImGui::SameLine();
    if (ImGui::Button("Preset: Mosaic")) {
        s_fx = PostFXParams{};
        s_fx.mosaicBlocks = 40.0f;
        applyFx();
    }
    if (ImGui::Button("Preset: Combined")) {
        s_fx = PostFXParams{};
        s_fx.rgbShift = 0.01f;
        s_fx.mosaicBlocks = 32.0f;
        applyFx();
    }
    ImGui::SameLine();
    if (ImGui::Button("Preset: Neon Glow")) {
        s_fx = PostFXParams{};
        s_fx.bloomIntensity = 1.2f;
        s_fx.bloomThreshold = 0.6f;
        s_fx.bloomSpread = 1.5f;
        s_fx.rgbShift = 0.003f;
        s_fx.vignette = 0.35f;
        applyFx();
    }

    ImGui::Separator();
    ImGui::Text("Bursts (temporary RGB punch)");
    if (ImGui::Button("GOAL Burst (0.012 / 0.45s)"))
        PostProcess_TriggerBurst(0.012f, 0.45f);
    ImGui::SameLine();
    if (ImGui::Button("Strong Burst (0.025 / 0.6s)"))
        PostProcess_TriggerBurst(0.025f, 0.6f);

    ImGui::Spacing();
    ImGui::TextDisabled("Hotkeys: F9 = GOAL burst, F10 = toggle mosaic");

    ImGui::End();
}

void DrawShortcutHintsWindow(bool* open) {
    if (!open || !*open) return;

    ImGui::SetNextWindowSize(ImVec2(Ui(420.0f), 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Shortcut Hints", open)) {
        ImGui::End();
        return;
    }

    auto row = [](const char* keys, const char* desc) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(keys);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("%s", desc);
    };

    if (ImGui::BeginTable("##ShortcutHints", 2,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Editor");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("");

        row("F2", "Play in viewport / stop");
        row("F3", "Play fullscreen / stop");
        row("F6", "Toggle third-person cursor");
        row("F8", "Toggle job-system debug window");
        row("F", "Focus selected object in scene view");
        row("Ctrl+S", "Save scene");
        row("Ctrl+Shift+S", "Save project");
        row("Ctrl+Z / Ctrl+Y", "Undo / redo");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Play / Runtime");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("");

        row("F7", "Cycle time scale (1.0 ?? 0.5 ?? 0.25 ?? freeze)");
        row("F11", "Toggle collision visualizer");
        row("F12", "Step one frame while frozen");
        row("F9", "PostFX GOAL RGB burst");
        row("F10", "Toggle PostFX mosaic showcase");

        ImGui::EndTable();
    }

    ImGui::End();
}

void DrawRenderingSettingsBody() {
    ImGui::BeginDisabled(EngineGUI_IsPlaying());
    bool scene2D=SceneLoader_Is2D();
    if(ImGui::Checkbox("2D Scene",&scene2D)) {
        SceneLoader_SetExecutionMode(scene2D,!scene2D);
        EngineGUI_MarkSceneDirty();
    }
    if(!scene2D) {
        bool physics3D=SceneLoader_Uses3DPhysics();
        if(ImGui::Checkbox("3D Physics",&physics3D)) {
            SceneLoader_SetExecutionMode(false,physics3D); EngineGUI_MarkSceneDirty();
        }
    } else {
        bool physics2D=SceneLoader_Uses2DPhysics();
        if(ImGui::Checkbox("2D Physics (Box2D)",&physics2D)) {
            SceneLoader_SetExecutionMode(true,false,physics2D); EngineGUI_MarkSceneDirty();
        }
        ImGui::TextDisabled("Sprite rendering; 3D physics and effects disabled.");
    }
    ImGui::EndDisabled();
    RenderSettingsData& rs = RenderSettings_Get();
    static char cubemapBuf[256] = {};
    static std::string syncedCubemapPath;

    if (rs.skyboxCubemapPath != syncedCubemapPath) {
        syncedCubemapPath = rs.skyboxCubemapPath;
        strncpy_s(cubemapBuf, rs.skyboxCubemapPath.c_str(), sizeof(cubemapBuf) - 1);
    }

    ImGui::Text("Environment");
    ImGui::Separator();
    if (ImGui::Checkbox("Skybox", &rs.skyboxEnabled))
        RenderSettings_ReloadSkybox();

    ImGui::Text("Cubemap / panorama PNG");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##SkyboxCubemap", cubemapBuf, sizeof(cubemapBuf),
        ImGuiInputTextFlags_EnterReturnsTrue)) {
        rs.skyboxCubemapPath = cubemapBuf;
        syncedCubemapPath = rs.skyboxCubemapPath;
        RenderSettings_ReloadSkybox();
        EngineGUI_MarkSceneDirty();
    }
    {
        const char* skyExts[] = { ".dds", ".png" };
        std::string droppedPath;
        if (AcceptAssetPathDrop(skyExts, IM_ARRAYSIZE(skyExts), droppedPath) &&
            EngineGUI_ApplySkyboxCubemapPath(droppedPath)) {
            syncedCubemapPath = rs.skyboxCubemapPath;
            strncpy_s(cubemapBuf, rs.skyboxCubemapPath.c_str(), sizeof(cubemapBuf) - 1);
        }
    }
    if (panorama::IsEquirectSkyPath(rs.skyboxCubemapPath)) {
        ImGui::DragFloat("Vertical FOV", &rs.skyboxVerticalFov, 0.5f, 0.0f, 180.0f, "%.1f deg (0=auto)");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (!(rs.skyboxVerticalFov > 0.0f))
                rs.skyboxVerticalFov = 0.0f;
            else if (rs.skyboxVerticalFov > 180.0f)
                rs.skyboxVerticalFov = 180.0f;
            RenderSettings_ReloadSkybox();
            EngineGUI_MarkSceneDirty();
        }
        ImGui::TextDisabled("0 = 360*height/width, centered on the horizon. Width is always 360 deg.");
        const panorama::Prep& prep = panorama::CurrentPrep();
        if (!prep.isPanorama || !panorama::SameRomPath(prep.sourceRom, rs.skyboxCubemapPath)) {
            ImGui::TextDisabled("Equirectangular PNG cooks to a DDS cubemap on reload.");
        } else if (!prep.ok) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "Panorama failed");
            ImGui::TextWrapped("%s", prep.error.c_str());
        } else if (!prep.iblReady) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "IBL not matching sky");
            if (!prep.error.empty())
                ImGui::TextWrapped("%s", prep.error.c_str());
        } else {
            ImGui::TextDisabled("IBL baked from this panorama. BRDF LUT is unchanged.");
        }
    }

    ImGui::ColorEdit4("Tint", &rs.skyboxTint.x);
    ImGui::DragFloat("Exposure", &rs.skyboxExposure, 0.05f, 0.1f, 5.0f);
    ImGui::DragFloat("Rotation Y", &rs.skyboxRotationYDeg, 1.0f, -180.0f, 180.0f);
    if (ImGui::Checkbox("IBL", &rs.iblEnabled))
        RenderSettings_ReloadSkybox();
    ImGui::DragFloat("IBL Intensity", &rs.iblIntensity, 0.05f, 0.0f, 5.0f);
    ImGui::ColorEdit4("Clear Color", &rs.clearColor.x);

    ImGui::Text("Default Shader");
    {
        const int variantCount = Shader3DVariantCount();
        const Shader3DVariant currentVariant = Shader3DVariantFromId(rs.defaultShader);
        const std::string preview = ShaderVariantPreviewLabel(currentVariant);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##DefaultShader", preview.c_str())) {
            for (int i = 0; i < variantCount; ++i) {
                const Shader3DVariant v = Shader3DVariantByIndex(i);
                if (!Shader3DVariantIsUserVisible(v))
                    continue;
                const bool selected = (v == currentVariant);
                if (ImGui::Selectable(Shader3DVariantLabel(v), selected))
                    rs.defaultShader = Shader3DVariantId(v);
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled(
            "PBR (Cook-Torrance): assign via this Default Shader or a VisualComponent Shader override. "
            "Tune Material Roughness (0.1 vs 0.8) and Metallic (0 vs 1); compare side-by-side with Toon. "
            "Open rom/scene/scene_pbr_showcase.json for a ready grid.");
    }

    ImGui::Spacing();
    ImGui::Text("Shadows");
    ImGui::Separator();
    {
        const char* preview = RenderSettings_ShadowQualityLabel(rs.shadowQuality);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##ShadowQuality", preview)) {
            const ShadowQuality options[] = {
                ShadowQuality::Low,
                ShadowQuality::Medium,
                ShadowQuality::High
            };
            for (ShadowQuality q : options) {
                const bool selected = (q == rs.shadowQuality);
                if (ImGui::Selectable(RenderSettings_ShadowQualityLabel(q), selected)) {
                    if (rs.shadowQuality != q) {
                        rs.shadowQuality = q;
                        RenderSettings_ApplyShadowQuality();
                    }
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SliderFloat("Shadow Distance", &rs.shadowDistance, 0.0f, 300.0f,
            rs.shadowDistance > 0.0f ? "%.0f m" : "Preset");
        if (rs.shadowDistance > 0.0f && rs.shadowDistance < 5.0f)
            rs.shadowDistance = 5.0f;
        const ShadowQualityPreset preset = RenderSettings_GetShadowPreset();
        ImGui::TextDisabled("%u px  |  %d cascades  |  range %.0f",
            preset.mapSize, preset.cascadeCount, preset.maxDistance);
    }

    ImGui::Spacing();
    ImGui::Text("Culling");
    ImGui::Separator();
    ImGui::Checkbox("Frustum Culling (3D)", &rs.frustumCullingEnabled);
    {
        const FrustumCullStats cull = FrustumCull_GetLastStats();
        ImGui::TextDisabled("frustum  considered %d  |  culled %d  |  drawn %d",
            cull.considered, cull.culled, cull.drawn);
        ImGui::TextDisabled("Main camera pass only.");
    }
    ImGui::Checkbox("Shadow Cascade Culling", &rs.shadowCascadeCullingEnabled);
    {
        const ShadowCascadeCullStats sc = ShadowCascadeCull_GetLastStats();
        ImGui::TextDisabled("shadow   considered %d  |  culled %d  |  drawn %d",
            sc.considered, sc.culled, sc.drawn);
        const int cascades = ShadowCascadeCull_GetLastCascadeCount();
        for (int i = 0; i < cascades; ++i) {
            const ShadowCascadeCullStats cs = ShadowCascadeCull_GetLastCascadeStats(i);
            ImGui::TextDisabled("  cascade %d  c:%d x:%d d:%d",
                i, cs.considered, cs.culled, cs.drawn);
        }
        ImGui::TextDisabled("Per-cascade light ortho; open near toward light.");
    }
    ImGui::Checkbox("Sprite Culling (2D)", &rs.spriteCullingEnabled);
    {
        const SpriteCullStats sc = SpriteCull_GetLastStats();
        ImGui::TextDisabled("sprite   considered %d  |  culled %d  |  drawn %d",
            sc.considered, sc.culled, sc.drawn);
        ImGui::TextDisabled("Ortho view-rect; render submit only.");
    }
    ImGui::Checkbox("Mesh LOD (3D)", &rs.lodEnabled);
    {
        int force = Lod_GetForceLevel();
        if (ImGui::SliderInt("Force LOD Level", &force, -1, lod_select::kMaxForcedLevel,
                force < 0 ? "Auto" : "L%d"))
            Lod_SetForceLevel(force);
        ImGui::TextDisabled("Every LOD mesh uses this level (limited to what it has). Not saved into the scene.");
        bool showRanges = Lod_GetShowRanges();
        if (ImGui::Checkbox("Show LOD Ranges (selected)", &showRanges))
            Lod_SetShowRanges(showRanges);
        ImGui::TextDisabled("Edit view: wire spheres at the selected object's switch distances (yellow / orange / red = L1 / L2 / L3, cyan = cull distance).");
    }
    ImGui::Checkbox("LOD Debug Labels", &rs.lodDebugLabels);
    ImGui::TextDisabled("Text above each LOD mesh: level, distance, projected screen height. Play only.");
    ImGui::SliderInt("Shadow LOD Offset", &rs.shadowLodOffset, 0, lod_select::kMaxShadowLodOffset);
    ImGui::TextDisabled("Shadow casters use this many LOD levels lower than the camera. 0 = same level.");
    ImGui::SliderFloat("LOD Bias", &rs.lodBias, 0.1f, 10.0f, "%.2f",
        ImGuiSliderFlags_Logarithmic);
    {
        const LodStats lod = Lod_GetLastStats();
        int lodFar = 0;
        for (int level = 2; level < kLodMaxLevels; ++level)
            lodFar += lod.drawn[level];
        ImGui::TextDisabled("lod      considered %d  |  culled %d  |  L0 %d  L1 %d  L2+ %d",
            lod.considered, lod.culledByDistance, lod.drawn[0], lod.drawn[1], lodFar);
        ImGui::TextDisabled("Bias > 1 switches to lower detail sooner. Play only.");
    }
    ImGui::Checkbox("Off-screen Animation Culling (3D)", &rs.animCullingEnabled);
    {
        const AnimCullStats anim = Lod_GetLastAnimStats();
        ImGui::TextDisabled("anim     evaluated %d  |  skipped %d", anim.updated, anim.skipped);
        ImGui::TextDisabled("Animated Visuals no pass drew last frame skip bone sampling (clip time still advances). Play only.");
    }
    ImGui::Checkbox("Billboard / Particle Culling (3D)", &rs.billboardCullingEnabled);
    ImGui::Checkbox("Effect Culling (3D)", &rs.effectCullingEnabled);
    ImGui::SliderFloat("Effect Cull Radius", &rs.effectCullRadius,
        cull_settings::kMinEffectCullRadius, cull_settings::kMaxEffectCullRadius, "%.1f m",
        ImGuiSliderFlags_Logarithmic);
    {
        const FxCullStats fx = FxCull_GetLastStats();
        ImGui::TextDisabled("billboard considered %d  |  culled %d", fx.billboardConsidered, fx.billboardCulled);
        ImGui::TextDisabled("particle  considered %d  |  culled %d", fx.particleConsidered, fx.particleCulled);
        ImGui::TextDisabled("effect    considered %d  |  culled %d", fx.effectConsidered, fx.effectCulled);
        ImGui::TextDisabled("Effects have no authored bounds: radius x scale around the effect position. Larger radius = fewer pops.");
    }
    ImGui::Checkbox("Light Selection (3D)", &rs.lightSelectionEnabled);
    {
        const SceneLightingStats ls = SceneLighting_GetLastStats();
        ImGui::TextDisabled("lights   enabled %d  |  used %d  (max %d)",
            ls.total, ls.used, Lighting::MAX_LIGHTS);
        ImGui::TextDisabled("Over the slot limit, the most influential lights at the camera win. Off = first in scene order.");
    }
    ImGui::SliderFloat("Cull Distance Scale", &rs.cullDistanceScale, 0.1f, 10.0f, "%.2f",
        ImGuiSliderFlags_Logarithmic);
    ImGui::TextDisabled("Multiplies every visual.cullDistance (> 1 keeps objects visible longer).");

    ImGui::Spacing();
    ImGui::Text("Fog");
    ImGui::Separator();
    ImGui::Checkbox("Height Fog", &rs.fogEnabled);
    ImGui::ColorEdit3("Fog Color", &rs.fogColor.x);
    ImGui::DragFloat("Density", &rs.fogDensity, 0.001f, 0.0f, 1.0f, "%.4f");
    ImGui::DragFloat("Start", &rs.fogStart, 0.5f, 0.0f, 500.0f);
    ImGui::DragFloat("Height", &rs.fogHeight, 0.1f, -50.0f, 200.0f);
    ImGui::DragFloat("Height Falloff", &rs.fogHeightFalloff, 0.01f, 0.0f, 5.0f, "%.3f");

    ImGui::Spacing();
    ImGui::TextDisabled(
        "A DDS cubemap loads directly. A PNG panorama (width = 360 deg; height <= width/2) cooks into rom/skybox/cache/. Drag a .png/.dds from the Asset Browser onto the path field, or right-click Set as > Skybox.");
}

