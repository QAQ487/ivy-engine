// texture_cook_policy.h
// Pure logic for the package-time texture cook: which rom textures are
// compressed to BC7 .dds when a playable package is built, and how the runtime
// finds the .dds when the source PNG/JPG/BMP is not in the package.
// No Windows or D3D dependency, so it is unit-testable on Linux.

#pragma once

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace texture_cook {

// BC formats need a base mip whose width and height are multiples of 4.
inline bool IsBlockCompressibleSize(std::size_t width, std::size_t height) {
    return width >= 4 && height >= 4 && (width % 4) == 0 && (height % 4) == 0;
}

namespace detail {

template <class Char>
inline Char Lower(Char c) {
    return (c >= Char('A') && c <= Char('Z')) ? static_cast<Char>(c - Char('A') + Char('a')) : c;
}

template <class Char>
inline bool ExtensionEquals(const std::basic_string<Char>& ext, const char* lowerLiteral) {
    std::size_t i = 0;
    for (; lowerLiteral[i]; ++i) {
        if (i >= ext.size() || Lower(ext[i]) != static_cast<Char>(lowerLiteral[i]))
            return false;
    }
    return i == ext.size();
}

// Position of the '.' that starts the extension of the last path segment, or npos.
template <class Char>
inline std::size_t ExtensionPos(const std::basic_string<Char>& path) {
    const std::size_t slash = path.find_last_of(std::basic_string<Char>{Char('/'), Char('\\')});
    const std::size_t dot = path.find_last_of(Char('.'));
    if (dot == std::basic_string<Char>::npos)
        return dot;
    if (slash != std::basic_string<Char>::npos && dot < slash)
        return std::basic_string<Char>::npos;
    return dot;
}

}  // namespace detail

// Source image formats the cook converts (case-insensitive extension, with dot).
template <class Char>
inline bool IsCookableExtension(const std::basic_string<Char>& ext) {
    return detail::ExtensionEquals(ext, ".png") || detail::ExtensionEquals(ext, ".jpg") ||
           detail::ExtensionEquals(ext, ".jpeg") || detail::ExtensionEquals(ext, ".bmp");
}

// "rom/tex/a.PNG" -> "rom/tex/a.dds". Empty when the path is not a cookable image
// (including a path that is already .dds), so callers never loop on the fallback.
template <class Char>
inline std::basic_string<Char> DdsSiblingPath(const std::basic_string<Char>& path) {
    const std::size_t dot = detail::ExtensionPos(path);
    if (dot == std::basic_string<Char>::npos)
        return {};
    if (!IsCookableExtension(path.substr(dot)))
        return {};
    std::basic_string<Char> out = path.substr(0, dot);
    for (const char* p = ".dds"; *p; ++p)
        out.push_back(static_cast<Char>(*p));
    return out;
}

// `romRelative` is the path below rom/ with '/' separators (e.g. "texture/a.png").
// Engine-owned folders and the host boot textures stay as source images: shaders,
// fonts, branding, skybox inputs/cache and editor state are not game textures, and
// the host / debug text loaders read their PNGs directly.
inline bool ShouldCookRomTexture(std::string_view romRelative) {
    const std::string path(romRelative);
    if (path.empty() || DdsSiblingPath(path).empty())
        return false;

    static const char* kExcludedDirs[] = {"shader", "font", "branding", "skybox", "editor"};
    const std::size_t slash = path.find('/');
    if (slash != std::string::npos) {
        const std::string top = path.substr(0, slash);
        for (const char* dir : kExcludedDirs) {
            if (top == dir)
                return false;
        }
    }

    static const char* kExcludedFiles[] = {"texture/fade.png", "texture/consolab_ascii_512.png"};
    std::string lower;
    lower.reserve(path.size());
    for (unsigned char c : path)
        lower.push_back(static_cast<char>(std::tolower(c)));
    for (const char* file : kExcludedFiles) {
        if (lower == file)
            return false;
    }
    return true;
}

}  // namespace texture_cook
