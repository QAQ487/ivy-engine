// builtin_assets.h
// Engine-owned files compiled into the exe, so they are never shipped in a
// package or copied into a content project. Regenerate the data with
// tools/embed_builtin_assets.py.

#pragma once

#include "engine/core/builtin_assets_data.h"

#include <cstddef>

namespace builtin_assets {

struct Asset {
    const unsigned char* data = nullptr;
    std::size_t size = 0;
};

// Debug text font: IBM Plex Mono Regular (SIL OFL 1.1, resources/font/OFL-IBMPlexMono.txt).
inline Asset DebugMonoFontTtf() {
    return Asset{kDebugMonoFontTtf, sizeof(kDebugMonoFontTtf)};
}

}  // namespace builtin_assets
