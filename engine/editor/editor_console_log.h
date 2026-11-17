// editor_console_log.h
// Thread-safe bounded console log ring buffer + filter helpers (Linux-testable).
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

enum class EditorConsoleLevel : int {
    Info = 0,
    Warning = 1,
    Error = 2,
};

struct EditorConsoleEntry {
    EditorConsoleLevel level = EditorConsoleLevel::Info;
    std::int64_t timestampMs = 0;
    std::string text;
    int repeatCount = 1;
};

struct EditorConsoleFilter {
    bool showInfo = true;
    bool showWarning = true;
    bool showError = true;
    std::string search; // case-insensitive substring; empty = match all
};

// Heuristic level from message text (cheap; unknown → Info).
inline EditorConsoleLevel InferConsoleLevel(std::string_view text)
{
    auto has = [&](const char* needle) -> bool {
        if (!needle || !*needle)
            return false;
        const size_t n = std::strlen(needle);
        if (text.size() < n)
            return false;
        for (size_t i = 0; i + n <= text.size(); ++i) {
            bool ok = true;
            for (size_t j = 0; j < n; ++j) {
                const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i + j])));
                const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(needle[j])));
                if (a != b) {
                    ok = false;
                    break;
                }
            }
            if (ok)
                return true;
        }
        return false;
    };

    if (has("error") || has("fail") || has("fatal") || has("exception"))
        return EditorConsoleLevel::Error;
    if (has("warning") || has("warn"))
        return EditorConsoleLevel::Warning;
    return EditorConsoleLevel::Info;
}

inline bool ConsoleEntryPassesFilter(const EditorConsoleEntry& e, const EditorConsoleFilter& f)
{
    switch (e.level) {
    case EditorConsoleLevel::Info:
        if (!f.showInfo) return false;
        break;
    case EditorConsoleLevel::Warning:
        if (!f.showWarning) return false;
        break;
    case EditorConsoleLevel::Error:
        if (!f.showError) return false;
        break;
    }
    if (f.search.empty())
        return true;

    // Case-insensitive substring search (UTF-8 safe for ASCII needles; CJK search is exact-byte).
    auto asciiLower = [](unsigned char c) -> char {
        return static_cast<char>(std::tolower(c));
    };
    const std::string& hay = e.text;
    const std::string& needle = f.search;
    if (needle.size() > hay.size())
        return false;
    for (size_t i = 0; i + needle.size() <= hay.size(); ++i) {
        bool ok = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            const unsigned char a = static_cast<unsigned char>(hay[i + j]);
            const unsigned char b = static_cast<unsigned char>(needle[j]);
            // Only fold ASCII; multi-byte UTF-8 (CJK) compared as-is.
            const char ca = (a < 128) ? asciiLower(a) : static_cast<char>(a);
            const char cb = (b < 128) ? asciiLower(b) : static_cast<char>(b);
            if (ca != cb) {
                ok = false;
                break;
            }
        }
        if (ok)
            return true;
    }
    return false;
}

// Bounded ring of entries. Push is thread-safe; snapshot for UI is a mutexed copy.
class EditorConsoleBuffer {
public:
    explicit EditorConsoleBuffer(size_t capacity = 2000)
        : m_capacity(capacity < 1 ? 1 : capacity)
    {
        m_entries.reserve(m_capacity);
    }

    void SetCapacity(size_t capacity)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_capacity = capacity < 1 ? 1 : capacity;
        while (m_entries.size() > m_capacity)
            m_entries.erase(m_entries.begin());
    }

    size_t Capacity() const { return m_capacity; }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.clear();
        ++m_generation;
    }

    void Push(std::string_view text, EditorConsoleLevel level, std::int64_t timestampMs)
    {
        // Trim trailing CR/LF only.
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
            text.remove_suffix(1);
        if (text.empty())
            return;

        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_entries.empty() && m_entries.back().text == text &&
            m_entries.back().level == level) {
            m_entries.back().repeatCount += 1;
            m_entries.back().timestampMs = timestampMs;
            ++m_generation;
            return;
        }

        EditorConsoleEntry e;
        e.level = level;
        e.timestampMs = timestampMs;
        e.text.assign(text.data(), text.size());
        e.repeatCount = 1;
        m_entries.push_back(std::move(e));
        while (m_entries.size() > m_capacity)
            m_entries.erase(m_entries.begin());
        ++m_generation;
    }

    void PushAutoLevel(std::string_view text, std::int64_t timestampMs)
    {
        Push(text, InferConsoleLevel(text), timestampMs);
    }

    std::uint64_t Generation() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_generation;
    }

    size_t Size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_entries.size();
    }

    // Copy entries that pass the filter (stable order, oldest first).
    std::vector<EditorConsoleEntry> Snapshot(const EditorConsoleFilter& filter) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<EditorConsoleEntry> out;
        out.reserve(m_entries.size());
        for (const auto& e : m_entries) {
            if (ConsoleEntryPassesFilter(e, filter))
                out.push_back(e);
        }
        return out;
    }

    // All entries (for Copy all).
    std::vector<EditorConsoleEntry> SnapshotAll() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_entries;
    }

private:
    mutable std::mutex m_mutex;
    size_t m_capacity = 2000;
    std::vector<EditorConsoleEntry> m_entries;
    std::uint64_t m_generation = 0;
};

inline std::string FormatConsoleEntryLine(const EditorConsoleEntry& e)
{
    char timeBuf[32];
    const std::int64_t ms = e.timestampMs;
    const std::int64_t sec = ms / 1000;
    const int millis = static_cast<int>(ms % 1000);
    const int s = static_cast<int>(sec % 60);
    const int m = static_cast<int>((sec / 60) % 60);
    const int h = static_cast<int>((sec / 3600) % 24);
    std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d.%03d", h, m, s, millis);

    const char* levelTag = "INF";
    switch (e.level) {
    case EditorConsoleLevel::Warning: levelTag = "WRN"; break;
    case EditorConsoleLevel::Error: levelTag = "ERR"; break;
    default: break;
    }

    std::string line;
    line.reserve(e.text.size() + 48);
    line.append(timeBuf);
    line.push_back(' ');
    line.append(levelTag);
    line.push_back(' ');
    line.append(e.text);
    if (e.repeatCount > 1) {
        char rep[32];
        std::snprintf(rep, sizeof(rep), " (x%d)", e.repeatCount);
        line.append(rep);
    }
    return line;
}
