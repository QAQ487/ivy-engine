// utf8_path.h
// Construct std::filesystem::path from UTF-8 bytes (C++20; replaces u8path).

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
