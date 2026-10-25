#include "engine/scene/scene_file_monitor.h"
#include "engine/scene/scene_json_validation.h"
#include <cassert>
#include <chrono>
#include <iostream>

int main() {
    namespace fs = std::filesystem;
    const auto directory = fs::temp_directory_path() /
        ("ivy-scene-monitor-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(directory);
    const auto path = (directory / "scene.json").string();
    int stamp = 0;
    auto write = [&](const std::string& content) {
        { std::ofstream file(path, std::ios::binary); file << content; }
        fs::last_write_time(path, fs::file_time_type{} + std::chrono::seconds(++stamp));
    };
    SceneFileMonitor monitor;
    write("baseline"); monitor.SetBaseline(path, "baseline");
    assert(!monitor.Poll(0.5));
    write("first"); assert(!monitor.Poll(0.5));
    assert(!monitor.Poll(0.2));
    write("latest"); assert(!monitor.Poll(0.3));
    auto version = monitor.Poll(0.5);
    assert(version && version->content == "latest");
    assert(!monitor.Poll(0.5)); // failed/ignored content is not retried
    write("latest"); assert(!monitor.Poll(0.5)); assert(!monitor.Poll(0.5));
    write("self save"); monitor.SetBaseline(path, "self save");
    assert(!monitor.Poll(0.5));
    // Loading a snapshot and installing its baseline can race another disk write.
    write("newer than snapshot"); monitor.SetBaseline(path, "old snapshot");
    version = monitor.Poll(0.5); assert(version && version->content == "newer than snapshot");
    fs::remove(path); assert(!monitor.Poll(0.5));
    write("recreated"); assert(!monitor.Poll(0.5));
    version = monitor.Poll(0.5); assert(version && version->content == "recreated");
    // Reverting to an earlier version is still a new change.
    write("baseline"); assert(!monitor.Poll(0.5));
    version = monitor.Poll(0.5); assert(version && version->content == "baseline");
    const auto second = (directory / "second.json").string();
    { std::ofstream file(second); file << "second"; }
    monitor.SetBaseline(second, "second"); write("old scene update");
    assert(!monitor.Poll(1.0));
    monitor.Reset(); assert(!monitor.Poll(1.0));
    using json = nlohmann::json;
    SceneJson_ValidateTypes(json{{"x", 2}}, json{{"x", 1.0}}, "transform");
    bool rejected = false;
    try { SceneJson_ValidateTypes(json{{"x", "bad"}}, json{{"x", 1.0}}, "transform"); }
    catch (const std::exception&) { rejected = true; }
    assert(rejected);
    fs::remove_all(directory);
    std::cout << "SCENE_FILE_MONITOR_CHECK PASS: debounce/coalescing/dedup/save/recreation/path reset/type errors\n";
}
