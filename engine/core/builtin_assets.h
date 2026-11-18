// builtin_assets.h
// Engine-owned files compiled into the exe, so they are never shipped in a
// package or copied into a content project. Regenerate the data with
// tools/embed_builtin_assets.py.

#pragma once

#include "engine/core/builtin_assets_data.h"

#include <cstddef>
#include <string>

namespace builtin_assets {

struct Asset {
    const unsigned char* data = nullptr;
    std::size_t size = 0;
};

// Debug text font: IBM Plex Mono Regular (SIL OFL 1.1, resources/font/OFL-IBMPlexMono.txt).
inline Asset DebugMonoFontTtf() {
    return Asset{kDebugMonoFontTtf, sizeof(kDebugMonoFontTtf)};
}

// Textures that answer to a rom/ path when no file exists there (project or resources/).
// A real file always wins, so a project can still override one. Keep the list in step
// with kBuiltinRomPaths in tools/materialize_engine_defaults.py (checked by
// tools/check_runtime_host_assets.py): those paths are never copied into a package.
struct TextureEntry {
    const char* romPath;  // lower case, '/' separators
    const unsigned char* data;
    std::size_t size;
};

inline const TextureEntry* TextureEntries(std::size_t* count) {
    static const TextureEntry kEntries[] = {
        {"rom/branding/ivy_logo.png", kBrandLogoPng, sizeof(kBrandLogoPng)},
        {"rom/skybox/ibl/brdf_lut.png", kBrdfLutPng, sizeof(kBrdfLutPng)},
    };
    *count = sizeof(kEntries) / sizeof(kEntries[0]);
    return kEntries;
}

// Case-insensitive, accepts '\\'. Only ASCII paths can match. `out` may be null.
template <class Char>
inline bool FindTexture(const std::basic_string<Char>& romPath, Asset* out) {
    std::string key;
    key.reserve(romPath.size());
    for (Char c : romPath) {
        const unsigned long v = static_cast<unsigned long>(c);
        if (v > 127)
            return false;
        char ch = static_cast<char>(v);
        if (ch == '\\')
            ch = '/';
        else if (ch >= 'A' && ch <= 'Z')
            ch = static_cast<char>(ch - 'A' + 'a');
        key.push_back(ch);
    }
    std::size_t count = 0;
    const TextureEntry* entries = TextureEntries(&count);
    for (std::size_t i = 0; i < count; ++i) {
        if (key == entries[i].romPath) {
            if (out) {
                out->data = entries[i].data;
                out->size = entries[i].size;
            }
            return true;
        }
    }
    return false;
}

}  // namespace builtin_assets
