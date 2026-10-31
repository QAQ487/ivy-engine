// env_var.h
// Portable getenv wrapper. Uses _dupenv_s on Windows (avoids C4996).

#pragma once

#include <cstdlib>
#include <optional>
#include <string>

inline std::optional<std::string> GetEnv(const char* name) {
    if (!name)
        return std::nullopt;
#ifdef _WIN32
    char* buf = nullptr;
    size_t len = 0;
    if (_dupenv_s(&buf, &len, name) != 0 || buf == nullptr)
        return std::nullopt;
    std::string value(buf);
    free(buf);
    return value;
#else
    if (const char* value = std::getenv(name))
        return std::string(value);
    return std::nullopt;
#endif
}
