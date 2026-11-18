// content_monitor_check.cpp
// Validates content file monitor hot-reload behavior.

#include "engine/editor/content_file_monitor.h"
#include "engine/core/utf8_path.h"
#include <cassert>
#include <iostream>
#include <thread>
int main(int argc, char** argv) {
    if (argc > 1) {
        const auto start = std::chrono::steady_clock::now();
        const auto snapshot = ContentFileMonitor::Scan(PathFromUtf8(argv[1]));
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
    std::filesystem::create_directories(root / "editor");
    { std::ofstream(root / "texture/nested/a.png") << "first"; std::ofstream(root / "shader/a.hlsl") << "ignored"; std::ofstream(root / "texture/save.tmp") << "ignored"; std::ofstream(root / "editor/imgui_layout.ini") << "ignored"; }
    auto disk = ContentFileMonitor::Scan(root);
    assert(disk.count("texture/nested/a.png") == 1 && !disk.count("shader/a.hlsl") && !disk.count("texture/save.tmp") && !disk.count("editor/") && !disk.count("editor/imgui_layout.ini"));
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
    assert(detected); // Native notification catches same-stamp overwrite without an audit wait.
    // CJK filenames (Explorer " - 複製" copy suffix and a short name) must scan via UTF-8.
    const std::string copyLeaf = "hp_fill - \xE8\xA4\x87\xE8\xA3\xBD.png";
    const std::string testLeaf = "\xE6\xB8\xAC\xE8\xA9\xA6.png";
    const std::string copyRel = "texture/" + copyLeaf;
    const std::string testRel = "texture/" + testLeaf;
    { std::ofstream(root / PathFromUtf8(copyRel)) << "cjk-copy"; }
    { std::ofstream(root / PathFromUtf8(testRel)) << "cjk-test"; }
    const auto cjk = ContentFileMonitor::Scan(root);
    assert(cjk.count(copyRel) == 1 && cjk.count(testRel) == 1);
    assert(PathToUtf8(PathFromUtf8(copyRel).filename()) == copyLeaf);
    const auto second = root / "second-project";
    std::filesystem::create_directories(second);
    { std::ofstream(second / "new.json") << "{}"; }
    for (int n = 0; n < 16; ++n) { assert(live.Poll(second).empty()); std::this_thread::sleep_for(std::chrono::milliseconds(25)); }
    live.Stop();
    assert(live.Error().empty() && !live.Pending());
    // All removal targets below are fixtures below the unique temporary test root.
    std::filesystem::remove(second / "new.json"); std::filesystem::remove(second);
    std::filesystem::remove(root / PathFromUtf8(copyRel));
    std::filesystem::remove(root / PathFromUtf8(testRel));
    std::filesystem::remove(root / "texture/nested/a.png");
    std::filesystem::remove(root / "texture/nested");
    std::filesystem::remove(root / "texture/save.tmp"); std::filesystem::remove(root / "texture");
    std::filesystem::remove(root / "shader/a.hlsl"); std::filesystem::remove(root / "shader");
    std::filesystem::remove(root / "editor/imgui_layout.ini"); std::filesystem::remove(root / "editor");
    std::filesystem::remove(root);
    std::cout << "PASS: content debounce, batching, additions, renames, deletions, same-stamp overwrite, CJK names, 20 updates\n";
}
