// utf8_path.h
// Construct / stringify std::filesystem::path as UTF-8.
// On Windows, path::string() / path(std::string) use the ACP and can throw
// std::system_error for characters outside that code page (e.g. CJK names).
// C++17: u8path / u8string() as std::string. C++20: u8string / char8_t.

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

// Interpret narrow bytes as UTF-8. On Windows this preserves Unicode paths;
// constructing path from std::string alone may use the ACP.
inline std::filesystem::path PathFromUtf8(std::string_view utf8) {
#if defined(__cpp_lib_char8_t)
    return std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
#else
    return std::filesystem::u8path(std::string(utf8));
#endif
}

// UTF-8 bytes with generic ('/') separators. Prefer this over path::string()
// / generic_string(), which convert via the ACP on Windows and may throw.
inline std::string PathToUtf8(const std::filesystem::path& path) {
#if defined(__cpp_lib_char8_t)
    const auto bytes = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
#else
    return path.generic_u8string();
#endif
}
