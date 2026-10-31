// content_monitor_check.cpp
// Validates content file monitor hot-reload behavior.

#include "engine/editor/content_file_monitor.h"
#include <cassert>
#include <iostream>
#include <thread>
int main(int argc, char** argv) {
    if (argc > 1) {
        const auto start = std::chrono::steady_clock::now();
        const auto snapshot = ContentFileMonitor::Scan(std::filesystem::u8path(argv[1]));
        std::cout << "Scan " << snapshot.size() << " entries in " << std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count() << "s\n";
        return 0;
    }
    ContentFileMonitor monitor;
    ContentFileMonitor::Snapshot a;
    ContentFileMonitor::File f; f.size = 4; f.hash = 10;
    a["texture/a.png"] = f;
    assert(monitor.Observe(a, 0).empty());
    auto b = a; b["texture/b.png"] = f;
    assert(monitor.Observe(b, 1).empty());
    assert(monitor.Observe(b, 1.49).empty());
    auto changes = monitor.Observe(b, 1.5);
    assert(changes.size() == 1 && changes[0].added);
    assert(monitor.Observe(b, 2).empty());
    b["texture/a.png"].hash = 11;
    assert(monitor.Observe(b, 3).empty());
    b["texture/a.png"].hash = 12; // multiple writes reset the stability window
    assert(monitor.Observe(b, 3.4).empty());
    assert(monitor.Observe(b, 3.8).empty());
    changes = monitor.Observe(b, 3.9);
    assert(changes.size() == 1 && !changes[0].added && !changes[0].removed);
    b.erase("texture/a.png"); b["new/nested/c.png"] = f;
    assert(monitor.Observe(b, 4).empty());
    changes = monitor.Observe(b, 4.5);
    assert(changes.size() == 2); // rename is deletion + addition, batched once
    for (int i = 0; i < 20; ++i) {
        b["texture/b.png"].hash++;
        assert(monitor.Observe(b, 5 + i).empty());
        assert(monitor.Observe(b, 5.5 + i).size() == 1);
    }
    auto root = std::filesystem::temp_directory_path() / ("ivy_content_check_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root / "texture/nested");
    std::filesystem::create_directories(root / "shader");
    { std::ofstream(root / "texture/nested/a.png") << "first"; std::ofstream(root / "shader/a.hlsl") << "ignored"; std::ofstream(root / "texture/save.tmp") << "ignored"; }
    auto disk = ContentFileMonitor::Scan(root);
    assert(disk.count("texture/nested/a.png") == 1 && !disk.count("shader/a.hlsl") && !disk.count("texture/save.tmp"));
    auto stamp = std::filesystem::last_write_time(root / "texture/nested/a.png");
    { std::ofstream(root / "texture/nested/a.png") << "other"; }
    std::filesystem::last_write_time(root / "texture/nested/a.png", stamp);
    assert(disk != ContentFileMonitor::Scan(root)); // same size/time overwrite still detected
    ContentFileMonitor live;
    for (int n = 0; n < 16; ++n) { assert(live.Poll(root).empty()); std::this_thread::sleep_for(std::chrono::milliseconds(25)); }
    { std::ofstream(root / "texture/nested/a.png") << "again"; }
    std::filesystem::last_write_time(root / "texture/nested/a.png", stamp);
    bool detected = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto batch = live.Poll(root);
        if (!batch.empty()) { detected = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    assert(detected); // Native notification catches same-stamp replacement without an audit wait.
    const auto second = root / "second-project";
    std::filesystem::create_directories(second);
    { std::ofstream(second / "new.json") << "{}"; }
    for (int n = 0; n < 16; ++n) { assert(live.Poll(second).empty()); std::this_thread::sleep_for(std::chrono::milliseconds(25)); }
    live.Stop();
    assert(live.Error().empty() && !live.Pending());
    // All removal targets below are fixtures below the unique temporary test root.
    std::filesystem::remove(second / "new.json"); std::filesystem::remove(second);
    std::filesystem::remove(root / "texture/nested/a.png");
    std::filesystem::remove(root / "texture/nested");
    std::filesystem::remove(root / "texture/save.tmp"); std::filesystem::remove(root / "texture");
    std::filesystem::remove(root / "shader/a.hlsl"); std::filesystem::remove(root / "shader");
    std::filesystem::remove(root);
    std::cout << "PASS: content debounce, batching, additions, renames, deletions, same-stamp overwrite, 20 updates\n";
}
