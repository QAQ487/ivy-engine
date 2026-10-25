#pragma once
// Opt-in Debug host check. Uses only its own temporary scene files.
#include "engine/ecs/component_factory.h"
#include "engine/scene/prepared_scene.h"
#include <chrono>
#include <cstdlib>

namespace {
class SceneHotLoadThrowComponent : public Component {
public:
    const char* GetTypeName() const override { return "SceneHotLoadThrow"; }
    void Deserialize(const std::string&) override { throw std::runtime_error("injected deserialize failure"); }
};
}

static void SceneHotLoadCheck_Tick(double elapsed) {
    if (!std::getenv("IVY_SCENE_HOT_LOAD_CHECK")) return;
    namespace fs = std::filesystem;
    using json = nlohmann::json;
    static int phase = 0;
    static double total = 0, waited = 0;
    static std::string original, path;
    static GameObject* savedNode = nullptr;
    static Camera savedCamera;
    static int switchMode = 0, heldFrames = 0;
    if (phase < 0) return;
    total += elapsed; waited += elapsed;
    auto fail = [&](const char* message) {
        std::fprintf(stderr, "SCENE_HOT_LOAD_CHECK FAIL: phase=%d %s [%s]\n", phase, message, g_sceneReloadStatus.c_str());
        std::fprintf(stderr, "dirty=%d conflict=%d pending=%d monitor=%s current=%s nodes=%zu revision=%llu/%llu\n",
            int(EngineGUI_IsSceneDirty()), int(g_sceneReloadConflict.has_value()), int(g_sceneLoadPending),
            g_sceneFileMonitor.Path().c_str(), SceneLoader_GetCurrentScenePath().c_str(), g_sceneNodes.size(),
            g_monitoredSceneRevision, SceneLoader_GetContentRevision());
        std::fflush(stderr);
        if (!original.empty()) ProjectManager::Instance().SetLastOpenedScene(fs::path(original).filename().string());
        EngineGUI_ClearSceneDirty(); phase = -1; PostQuitMessage(2);
    };
    auto name = []() { return g_sceneNodes.empty() ? std::string{} : g_sceneNodes.front()->GetName(); };
    auto scene = [](int version) {
        return json{{"version", 4}, {"settings", {{"renderPath", "2d"}, {"physicsMode", "none"},
            {"render", {{"fogDensity", version / 10.0f}}}}},
            {"nodes", json::array({{{"name", "Version" + std::to_string(version)},
                {"components", {{"SceneTransform", {{"posX", float(version)}}}, {"visual", json::object()}}}}})},
            {"canvas", {{"elements", json::array({{{"type", "text"}, {"name", "TestHud"},
                {"text", "HUD" + std::to_string(version)}}})}}}};
    };
    auto write = [&](const json& root) { std::ofstream file(path, std::ios::binary); file << root.dump(); };
    auto next = [&](int target) { phase = target; waited = 0; };
    if (total > 90) { fail("timed out"); return; }
    if (phase == 14 || phase == 15) {
        const auto expected = switchMode == 0 ? PlayMode::Fullscreen : PlayMode::Viewport;
        if (EngineGUI_GetPlayMode() != expected || !EngineGUI_PreferPlayVisuals()
            || EngineGUI_IsEnabled() == (expected == PlayMode::Fullscreen)) {
            fail("Play scene switch changed presentation"); return;
        }
        if (g_sceneLoadPending || g_holdPlayVisuals) ++heldFrames;
    }
    static size_t startupPrevious = 0;
    static unsigned startupBuildFrames = 0;
    if (phase == 0 && g_scenePreparation && !SceneLoader_PreparationReading(*g_scenePreparation)) {
        const auto completed = SceneLoader_PreparationCompleted(*g_scenePreparation);
        if (completed < startupPrevious) { fail("startup object progress went backwards"); return; }
        if (completed > startupPrevious) ++startupBuildFrames;
        startupPrevious = completed;
    }
    if (IsSceneLoadBusy()) return;
    if (phase == 0) {
        if (!ProjectManager::Instance().IsOpen() || SceneLoader_GetCurrentScenePath().empty()) return;
        if (startupPrevious > 512 && startupBuildFrames < 2) {
            fail("large startup scene was not spread across frames"); return;
        }
        total = 0; // Startup loading has its own wall-clock allowance in the launcher.
        original = SceneLoader_GetCurrentScenePath();
        const auto folder = fs::temp_directory_path() / ("ivy-scene-hotload-" + std::to_string(GetCurrentProcessId()));
        fs::create_directories(folder); path = (folder / "scene.json").string();
        write(scene(1)); EngineGUI_LoadFromFileCore(path.c_str()); next(1); return;
    }
    if (phase == 1 && name() == "Version1") {
        savedNode = g_sceneNodes.front().get();
        auto* canvas = SceneCanvas_GetCanvas();
        const auto content = SceneLoader_GetCurrentSceneContent();
        const auto fog = RenderSettings_GetConst().fogDensity;
        std::string error;
        ComponentFactory::Instance().Register("SceneHotLoadThrow", [] { return std::make_unique<SceneHotLoadThrowComponent>(); });
        auto malformed = scene(9); malformed["nodes"][0]["components"]["SceneTransform"]["posX"] = "bad";
        auto badCanvas = scene(9); badCanvas["canvas"]["elements"][0]["fontSize"] = "bad";
        auto badSettings = scene(9); badSettings["settings"]["render"]["fogDensity"] = "bad";
        auto throwing = scene(9); throwing["nodes"][0]["components"]["SceneHotLoadThrow"] = json::object();
        for (const auto& invalid : {std::string("{"), malformed.dump(), badCanvas.dump(), badSettings.dump(), throwing.dump()}) {
            if (SceneLoader_PrepareFromContent(path, invalid, error) || error.empty()
                || g_sceneNodes.front().get() != savedNode || SceneCanvas_GetCanvas() != canvas
                || SceneLoader_GetCurrentSceneContent() != content || RenderSettings_GetConst().fogDensity != fog) {
                fail("failed preparation changed live scene"); return;
            }
        }
        // One-node ticks must pause inside a large child list, without changing the live scene.
        auto large = scene(8);
        auto& children = large["nodes"][0]["children"] = json::array();
        for (int i = 0; i < 256; ++i) children.push_back({{"name", "Child" + std::to_string(i)},
            {"components", {{"SceneTransform", {{"posX", float(i)}}}}},
            {"children", json::array({{{"name", "Grandchild"}, {"components", json::object()}}})}});
        const auto source = large.dump();
        auto incremental = SceneLoader_BeginPreparation(path, &source, false);
        size_t previous = 0; int ticks = 0;
        while (!SceneLoader_TickPreparation(*incremental, 0.0)) {
            const auto count = SceneLoader_PreparationCompleted(*incremental);
            if (count < previous || count > previous + 1 || g_sceneNodes.front().get() != savedNode) {
                fail("incremental preparation mutated live scene or skipped node budget"); return;
            }
            previous = count; ++ticks;
        }
        auto built = SceneLoader_PreparationResult(*incremental);
        auto synchronous = SceneLoader_PrepareFromContent(path, source, error);
        if (!built || !synchronous || ticks < 513 || SceneLoader_PreparationTotal(*incremental) != 513
            || built->nodes[0]->GetChildren().size() != 256
            || PrefabSerializer::Serialize(*built->nodes[0]) != PrefabSerializer::Serialize(*synchronous->nodes[0])) {
            fail("incremental hierarchy differs from synchronous preparation"); return;
        }
        auto missing = SceneLoader_BeginPreparation(path + ".missing", nullptr, false);
        while (!SceneLoader_TickPreparation(*missing)) {}
        if (SceneLoader_PreparationResult(*missing) || SceneLoader_PreparationError(*missing).empty()
            || g_sceneNodes.front().get() != savedNode) { fail("missing file transaction changed scene"); return; }
        GetCamera().position = {17, 23, -31}; GetCamera().at = {17, 23, 0};
        DebugCamera_SyncOrbitFromCurrentView(); savedCamera = GetCamera();
        EngineGUI_SetEditMode(EditorEditMode::UI); g_selNode = savedNode;
        write(scene(2)); next(2); return;
    }
    if (phase == 2 && name() == "Version2") {
        if (g_sceneReloadLight.failed || g_sceneReloadLight.brightUntil <= ImGui::GetTime()) {
            fail("scene success light did not brighten"); return;
        }
        auto* text = dynamic_cast<UIText*>(SceneCanvas_GetCanvas()->GetElement("TestHud"));
        const auto pos = GetCamera().position;
        if (!text || text->GetText() != "HUD2" || g_sceneNodes.front()->GetTransform()->localPosition.x != 2
            || std::abs(RenderSettings_GetConst().fogDensity - 0.2f) > 0.001f
            || EngineGUI_GetEditMode() != EditorEditMode::UI || g_selNode || g_selUiElement
            || EditorUndoStack::Instance().CanUndo()
            || std::abs(pos.x - savedCamera.position.x) > 0.01f || std::abs(pos.y - savedCamera.position.y) > 0.01f) {
            fail("reload did not replace content/preserve editor state"); return;
        }
        g_sceneNodes.front()->SetName("LocalEdit"); EngineGUI_MarkSceneDirty();
        write(scene(3)); next(3); return;
    }
    if (phase == 3 && g_sceneReloadConflict) {
        auto* modal = ImGui::FindWindowByName("SceneJsonChangedPopup");
        if (!modal || !modal->Active) return;
        ResolveSceneReloadConflict(false); next(4); return;
    }
    if (phase == 4 && waited > 1.5) {
        if (name() != "LocalEdit" || !EngineGUI_IsSceneDirty() || g_sceneReloadConflict) { fail("keep Editor version failed"); return; }
        write(scene(4)); next(5); return;
    }
    if (phase == 5 && g_sceneReloadConflict) {
        auto* modal = ImGui::FindWindowByName("SceneJsonChangedPopup");
        if (!modal || !modal->Active) return;
        ResolveSceneReloadConflict(true); next(6); return;
    }
    if (phase == 6 && name() == "Version4") {
        if (EngineGUI_IsSceneDirty()) { fail("disk choice remained dirty"); return; }
        EngineGUI_StartPlay(PlayMode::Viewport); write(scene(5)); next(7); return;
    }
    if (phase == 7 && waited > 1.5) {
        if (!EngineGUI_IsPlaying() || name() != "Version4") { fail("hot-load ran during Play"); return; }
        if (GetReloadLightState(true, EngineGUI_PreferPlayVisuals(), false, g_sceneReloadLight) != ReloadLightState::Paused
            || GetReloadLightState(true, EngineGUI_PreferPlayVisuals(), false, g_scriptReloadLight) != ReloadLightState::Paused) {
            fail("reload lights did not pause during Play"); return;
        }
        EngineGUI_StopPlay(); next(8); return;
    }
    if (phase == 8 && name() == "Version5") {
        savedNode = g_sceneNodes.front().get();
        if (!EngineGUI_SaveToFile(path.c_str())) { fail("self save failed"); return; }
        next(9); return;
    }
    if (phase == 9 && waited > 1.5) {
        if (g_sceneNodes.front().get() != savedNode || g_sceneReloadConflict) { fail("self save triggered reload"); return; }
        { std::ofstream file(path); file << "{"; }
        next(10); return;
    }
    if (phase == 10 && waited > 1.5) {
        if (g_sceneNodes.front().get() != savedNode || name() != "Version5"
            || g_sceneReloadStatus.find("Scene reload failed:") != 0 || !g_sceneReloadLight.failed) {
            fail("invalid disk JSON damaged live scene or failed to set error light"); return;
        }
        write(scene(6)); next(11); return;
    }
    if (phase == 11 && name() == "Version6") {
        next(12); return;
    }
    if (phase == 12) {
        EngineGUI_StartPlay(switchMode == 0 ? PlayMode::Fullscreen : PlayMode::Viewport);
        heldFrames = 0; next(13); return;
    }
    if (phase == 13 && EngineGUI_IsPlaying() && waited > 0.1) {
        const auto second = (fs::path(path).parent_path() / "next.json").string();
        { std::ofstream file(second); file << scene(7).dump(); }
        SceneCanvas_RequestSceneFile(second, true); next(14); return;
    }
    if (phase == 14 && name() == "Version7" && EngineGUI_IsPlaying()) {
        if (!EngineGUI_LoadFromFile(path.c_str())) { fail("Play return load refused"); return; }
        EngineGUI_StartPlay(); next(15); return;
    }
    if (phase == 15 && name() == "Version6" && EngineGUI_IsPlaying()) {
        if (!heldFrames) { fail("Play scene switch never held presentation"); return; }
        EngineGUI_StopPlay();
        if (++switchMode == 2) { EngineGUI_LoadFromFileCore(original.c_str()); next(16); }
        else next(12);
        return;
    }
    if (phase == 16 && SceneLoader_GetCurrentScenePath() == original) {
        ScriptFactory::Instance().RequestReload("MissingIndicatorCheck", (fs::path(path).parent_path() / "missing.dll").string());
        next(17); return;
    }
    if (phase == 17) {
        if (!g_scriptReloadLight.failed || g_scriptReloadStatus.find("MissingIndicatorCheck") == std::string::npos) {
            fail("script failure did not set error light/detail"); return;
        }
        SceneLoader_ScanScriptDllsForChanges(true); next(18); return;
    }
    if (phase == 18 && !g_scriptReloadLight.failed) {
        if (g_scriptReloadLight.brightUntil <= ImGui::GetTime()) { fail("script recovery did not brighten light"); return; }
        std::printf("SCENE_HOT_LOAD_CHECK PASS: incremental nested hierarchy/budget/missing file/failed transactions/nodes/Canvas/settings/camera/selection/Undo/dirty choices/Play deferral/self save/fullscreen+viewport scene switches/scene+script status lights\n");
        std::fflush(stdout); phase = -1; PostQuitMessage(0);
    }
}
