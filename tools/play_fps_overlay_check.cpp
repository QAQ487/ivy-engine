#include "engine/core/play_fps_overlay.h"
#include <cassert>
#include <cstdio>

int main() {
    using namespace PlayFpsOverlay;
    using nlohmann::json;
    SetEnabled(false);
    assert(!ShouldDraw(false) && !ShouldDraw(true));
    SetEnabled(true);
    assert(!ShouldDraw(false) && ShouldDraw(true));

    json prefs{{"uiZoomPercent",125}};
    WritePrefs(prefs);
    assert(prefs["uiZoomPercent"] == 125 && prefs["showFpsWhenPlaying"] == true);
    SetEnabled(false);
    LoadPrefs(json::parse(prefs.dump()));
    assert(ShouldDraw(true));
    SetEnabled(false);
    WritePrefs(prefs);
    assert(prefs["showFpsWhenPlaying"] == false);

    for (const auto& invalid : {json{}, json::array(), json{{"uiZoomPercent",100}},
         json{{"showFpsWhenPlaying","true"}}, json{{"showFpsWhenPlaying",1}}}) {
        SetEnabled(true);
        LoadPrefs(invalid);
        assert(!ShouldDraw(true));
    }

    const char* path = "tools/phase6-test-build/play-fps-prefs.json";
    { std::ofstream file(path); file << R"({"showFpsWhenPlaying":true})"; }
    LoadFromFile(path);
    assert(ShouldDraw(true));
    { std::ofstream file(path); file << "invalid json"; }
    LoadFromFile(path);
    assert(!ShouldDraw(true));
    SetEnabled(true);
    LoadFromFile("tools/phase6-test-build/missing-play-fps-prefs.json");
    assert(!ShouldDraw(true));
    std::puts("PASS: play FPS toggle, persistence, legacy and malformed preferences");
}
