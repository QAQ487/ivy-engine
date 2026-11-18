// builtin_assets.h
// Engine-owned files compiled into the exe, so they are never shipped in a
// package or copied into a content project. Regenerate the data with
// tools/embed_builtin_assets.py.

#pragma once

#include "engine/core/builtin_assets_data.h"
#include "engine/core/builtin_paths.h"

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

// Each asset answers to its "builtin:" path (builtin_paths.h) and never touches the disk. The
// older "rom/..." spellings from before the "builtin:" prefix still work, but only when no
// file exists there (project or resources/), so a project can override those. Keep the "rom/"
// spellings in step with kBuiltinRomPaths in playable_package_builder.cpp and
// tools/materialize_engine_defaults.py (tools/check_runtime_host_assets.py checks it): those
// paths are never copied into a package.
struct TextureEntry {
    const char* romPath;  // lower case, '/' separators
    const unsigned char* data;
    std::size_t size;
};

inline const TextureEntry* TextureEntries(std::size_t* count) {
    static const TextureEntry kEntries[] = {
        {"builtin:branding/ivy_logo.png", kBrandLogoPng, sizeof(kBrandLogoPng)},
        {"builtin:ibl/brdf_lut.png", kBrdfLutPng, sizeof(kBrdfLutPng)},
        {"rom/branding/ivy_logo.png", kBrandLogoPng, sizeof(kBrandLogoPng)},
        {"rom/skybox/ibl/brdf_lut.png", kBrdfLutPng, sizeof(kBrdfLutPng)},
    };
    *count = sizeof(kEntries) / sizeof(kEntries[0]);
    return kEntries;
}

// Fonts that answer to a rom/ path when no file exists there. The default UI font is the
// same IBM Plex Mono as the debug text font. Same rules and checks as the textures above.
inline const TextureEntry* FontEntries(std::size_t* count) {
    static const TextureEntry kEntries[] = {
        {"builtin:font/ibmplexmono-regular.ttf", kDebugMonoFontTtf, sizeof(kDebugMonoFontTtf)},
        {"rom/font/ibmplexmono-regular.ttf", kDebugMonoFontTtf, sizeof(kDebugMonoFontTtf)},
    };
    *count = sizeof(kEntries) / sizeof(kEntries[0]);
    return kEntries;
}

namespace detail {

template <class Char>
inline bool Lookup(const std::basic_string<Char>& romPath, const TextureEntry* (*entries)(std::size_t*), Asset* out) {
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
    const TextureEntry* table = entries(&count);
    for (std::size_t i = 0; i < count; ++i) {
        if (key == table[i].romPath) {
            if (out) {
                out->data = table[i].data;
                out->size = table[i].size;
            }
            return true;
        }
    }
    return false;
}

}  // namespace detail

// Case-insensitive, accepts '\\'. Only ASCII paths can match. `out` may be null.
template <class Char>
inline bool FindTexture(const std::basic_string<Char>& romPath, Asset* out) {
    return detail::Lookup(romPath, &TextureEntries, out);
}

template <class Char>
inline bool FindFont(const std::basic_string<Char>& romPath, Asset* out) {
    return detail::Lookup(romPath, &FontEntries, out);
}

}  // namespace builtin_assets
