// utf8_path.h
// Construct / stringify std::filesystem::path as UTF-8 (C++20; replaces u8path).
// On Windows, path::string() / path(std::string) use the ACP and can throw
// std::system_error for characters outside that code page (e.g. CJK names).

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

// Interpret narrow bytes as UTF-8. On Windows this preserves Unicode paths;
// constructing path from std::string alone may use the ACP.
inline std::filesystem::path PathFromUtf8(std::string_view utf8) {
    return std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

// UTF-8 bytes with generic ('/') separators. Prefer this over path::string()
// / generic_string(), which convert via the ACP on Windows and may throw.
inline std::string PathToUtf8(const std::filesystem::path& path) {
    const auto bytes = path.generic_u8string();
    return std::string(bytes.begin(), bytes.end());
}
