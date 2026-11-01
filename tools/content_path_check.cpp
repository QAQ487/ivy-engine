// Linux-runnable checks for content_path.h. No Windows headers.

#include "engine/core/content_path.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static int g_fails = 0;

static void Expect(bool cond, const char* label) {
    if (cond)
        return;
    std::cerr << "FAIL: " << label << '\n';
    ++g_fails;
}

static bool SafeRom(const char* path) {
    std::string normalized;
    if (!content_path::IsSafeRomPath(path, &normalized))
        return false;
    return normalized.find('\\') == std::string::npos &&
           normalized.find("..") == std::string::npos;
}

int main() {
    Expect(SafeRom("rom/texture/sprite_demo/hp_fill.png"), "sprite path");
    Expect(SafeRom("rom/model/player/Y Bot_white.fbx"), "spaced model path");
    Expect(SafeRom("rom\\skybox\\MySkybox.dds"), "backslash rom path");

    Expect(!content_path::IsSafeRomPath("..", nullptr), "dotdot");
    Expect(!content_path::IsSafeRomPath("../rom/model/a.fbx", nullptr), "parent prefix");
    Expect(!content_path::IsSafeRomPath("rom/../../etc/passwd", nullptr), "rom parent escape");
    Expect(!content_path::IsSafeRomPath("rom/texture/../secret.png", nullptr), "mid escape");
    Expect(!content_path::IsSafeRomPath("/etc/passwd", nullptr), "absolute unix");
    Expect(!content_path::IsSafeRomPath("C:/Windows/notepad.exe", nullptr), "drive path");
    Expect(!content_path::IsSafeRomPath("rom//texture/a.png", nullptr), "empty segment");
    Expect(!content_path::IsSafeRomPath("rom/texture/a.png/", nullptr), "trailing slash");
    Expect(!content_path::IsSafeRomPath("", nullptr), "empty");
    Expect(!content_path::IsSafeRomPath("rom", nullptr), "rom alone");
    Expect(!content_path::IsSafeRomPath("ROM/texture/a.png", nullptr), "uppercase prefix");
    Expect(!content_path::IsSafeRelativePath("foo/../../bar", nullptr), "relative escape");

    const char* expectedDirs[] = {
        "branding", "font", "model", "physmat", "shader", "skybox", "texture",
    };
    Expect(content_path::kSharedRomDirCount == 7, "shared dir count");
    for (std::size_t i = 0; i < content_path::kSharedRomDirCount; ++i) {
        Expect(std::string(content_path::kSharedRomDirNames[i]) == expectedDirs[i],
               "shared dir name");
        Expect(content_path::IsSafeRelativePath(content_path::kSharedRomDirNames[i], nullptr),
               "shared dir is a single safe segment");
        Expect(std::string(content_path::kSharedRomDirNames[i]).find('/') == std::string::npos,
               "shared dir has no slash");
    }
    Expect(std::string(content_path::kInRepoTestbedDir) == "testbed", "testbed folder name");
    Expect(content_path::IsSafeRelativePath(content_path::kInRepoTestbedDir, nullptr),
           "testbed name is safe");

    const fs::path scratch = fs::temp_directory_path() / "ivy_content_path_check";
    fs::remove_all(scratch);
    const fs::path root = scratch / "repo";
    fs::create_directories(root / "rom" / "model");
    fs::create_directories(root / "testbed" / "rom" / "scene");
    fs::create_directories(scratch / "outside");
    Expect(content_path::IsInsideDirectory(root, root / "testbed"), "testbed inside repo");
    Expect(content_path::IsInsideDirectory(root / "rom", root / "rom" / "model"),
           "model inside rom");
    Expect(!content_path::IsInsideDirectory(root, root), "root is not inside itself");
    Expect(!content_path::IsInsideDirectory(root / "rom", scratch / "outside"),
           "outside path rejected");
    Expect(!content_path::IsInsideDirectory(root, root / "missing-child"),
           "missing path rejected");
    fs::remove_all(scratch);

    if (g_fails != 0) {
        std::cerr << g_fails << " failure(s)\n";
        return 1;
    }
    std::cout << "PASS: content path rules\n";
    return 0;
}
