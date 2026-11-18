// texture_cook.h
// Package-time texture cook: compress rom PNG/JPG/BMP to BC7 .dds and drop the
// source image from the package. The runtime finds the .dds through the
// DdsSiblingPath fallback (engine/core/texture_cook_policy.h), so scene, prefab
// and UI JSON keep naming the original image. Windows only (DirectXTex).

#pragma once

#include <filesystem>
#include <functional>
#include <string>

namespace texture_cook {

struct Stats {
    int cooked = 0;           // source image replaced by a .dds
    int skippedSize = 0;      // base mip is not a multiple of 4, kept as the source image
    int skippedExisting = 0;  // a sibling .dds already exists, both files kept
    int failed = 0;           // load / compress / save failed, source image kept
};

// Walks packageRom (the rom/ folder inside the package, never the content
// project) and cooks every file texture_cook::ShouldCookRomTexture accepts.
// Per-file problems are logged and leave that source image in place; the build
// continues. Returns false only when packageRom cannot be walked.
bool CookPackageTextures(const std::filesystem::path& packageRom,
                         const std::function<void(const std::string&)>& log,
                         Stats* stats, std::string* error);

}  // namespace texture_cook
