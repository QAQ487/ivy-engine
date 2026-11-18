// host_text.h
// Passing text across the GameScripts.dll boundary without std::string:
// the host copies into a caller buffer and returns the full length, and the
// script side calls twice (length, then bytes). Used by the v51 save API.
// No Windows or engine dependency, so it is unit-testable on Linux.

#pragma once

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace host_text {

// Copies text into out (NUL-terminated, truncated to capacity - 1 bytes) when
// out is not null and capacity > 0. Returns the full length of text (capped at INT_MAX).
inline int CopyOut(const std::string& text, char* out, int capacity) {
    if (out && capacity > 0) {
        const std::size_t n = (std::min)(text.size(), static_cast<std::size_t>(capacity - 1));
        std::memcpy(out, text.data(), n);
        out[n] = '\0';
    }
    return static_cast<int>((std::min)(text.size(), static_cast<std::size_t>(INT_MAX)));
}

// getter(char* out, int capacity) -> length or -1. Returns false on -1.
template <class Getter>
bool ReadText(Getter getter, std::string* out) {
    const int length = getter(nullptr, 0);
    if (length < 0 || !out)
        return false;
    std::string buf(static_cast<std::size_t>(length) + 1, '\0');
    const int again = getter(&buf[0], static_cast<int>(buf.size()));
    if (again < 0)
        return false;
    buf.resize(static_cast<std::size_t>((std::min)(again, length)));
    *out = std::move(buf);
    return true;
}

inline std::string JoinLines(const std::vector<std::string>& items) {
    std::string joined;
    for (const std::string& item : items) {
        if (!joined.empty())
            joined += '\n';
        joined += item;
    }
    return joined;
}

inline std::vector<std::string> SplitLines(const std::string& joined) {
    std::vector<std::string> items;
    std::size_t start = 0;
    while (start < joined.size()) {
        const std::size_t end = joined.find('\n', start);
        items.push_back(joined.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return items;
}

}  // namespace host_text
