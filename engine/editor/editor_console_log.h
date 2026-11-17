// editor_console_log.h
// Thread-safe bounded console log ring buffer + filter helpers (Linux-testable).
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

inline constexpr size_t kEditorConsoleEntryMaxBytes = 4096;

enum class EditorConsoleLevel : int {
    Info = 0,
    Warning = 1,
    Error = 2,
};

struct EditorConsoleEntry {
    std::uint64_t id = 0;
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

inline bool ConsoleFilterEqual(const EditorConsoleFilter& a, const EditorConsoleFilter& b)
{
    return a.showInfo == b.showInfo && a.showWarning == b.showWarning &&
           a.showError == b.showError && a.search == b.search;
}

// Word-boundary-ish match so "0 errors" / "failover" are not tagged ERR.
inline bool HasLevelKeyword(std::string_view text, const char* needle)
{
    if (!needle || !*needle)
        return false;
    const size_t n = std::strlen(needle);
    if (text.size() < n)
        return false;
    auto lower = [](unsigned char c) -> char {
        return static_cast<char>(std::tolower(c));
    };
    auto isWord = [](unsigned char c) -> bool {
        return std::isalnum(c) != 0 || c == '_';
    };
    for (size_t i = 0; i + n <= text.size(); ++i) {
        bool ok = true;
        for (size_t j = 0; j < n; ++j) {
            if (lower(static_cast<unsigned char>(text[i + j])) !=
                lower(static_cast<unsigned char>(needle[j]))) {
                ok = false;
                break;
            }
        }
        if (!ok)
            continue;
        const bool leftOk = (i == 0) || !isWord(static_cast<unsigned char>(text[i - 1]));
        const bool rightOk = (i + n >= text.size()) ||
            !isWord(static_cast<unsigned char>(text[i + n]));
        if (leftOk && rightOk)
            return true;
    }
    return false;
}

// Heuristic level from message text (cheap; unknown → Info).
inline EditorConsoleLevel InferConsoleLevel(std::string_view text)
{
    if (HasLevelKeyword(text, "error") || HasLevelKeyword(text, "failed") ||
        HasLevelKeyword(text, "failure") || HasLevelKeyword(text, "fatal") ||
        HasLevelKeyword(text, "exception") || HasLevelKeyword(text, "fail"))
        return EditorConsoleLevel::Error;
    if (HasLevelKeyword(text, "warning") || HasLevelKeyword(text, "warn"))
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

// Truncate to maxBytes without splitting a UTF-8 multi-byte character.
inline void CapConsoleEntryText(std::string& text, size_t maxBytes = kEditorConsoleEntryMaxBytes)
{
    if (text.size() <= maxBytes)
        return;
    size_t cut = maxBytes;
    // Back up while on a UTF-8 continuation byte (10xxxxxx).
    while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0u) == 0x80u)
        --cut;
    text.resize(cut);
}

// Split on '\n', trim CR, drop empties, cap each piece. Pure helper for tests.
inline std::vector<std::string> SplitConsoleLogLines(std::string_view text)
{
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string_view::npos)
            end = text.size();
        std::string_view piece = text.substr(start, end - start);
        while (!piece.empty() && piece.back() == '\r')
            piece.remove_suffix(1);
        if (!piece.empty()) {
            std::string s(piece);
            CapConsoleEntryText(s);
            lines.push_back(std::move(s));
        }
        if (end == text.size())
            break;
        start = end + 1;
    }
    return lines;
}

// Bounded deque ring. Push is thread-safe; Snapshot caches by generation+filter.
class EditorConsoleBuffer {
public:
    explicit EditorConsoleBuffer(size_t capacity = 2000)
        : m_capacity(capacity < 1 ? 1 : capacity)
    {
    }

    void SetCapacity(size_t capacity)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_capacity = capacity < 1 ? 1 : capacity;
        while (m_entries.size() > m_capacity)
            m_entries.pop_front();
        InvalidateCacheLocked();
    }

    size_t Capacity() const { return m_capacity; }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.clear();
        InvalidateCacheLocked();
    }

    void Push(std::string_view text, EditorConsoleLevel level, std::int64_t timestampMs)
    {
        const std::vector<std::string> lines = SplitConsoleLogLines(text);
        if (lines.empty())
            return;

        std::lock_guard<std::mutex> lock(m_mutex);
        for (const std::string& line : lines)
            PushLineLocked(line, level, timestampMs);
    }

    void PushAutoLevel(std::string_view text, std::int64_t timestampMs)
    {
        const std::vector<std::string> lines = SplitConsoleLogLines(text);
        if (lines.empty())
            return;

        std::lock_guard<std::mutex> lock(m_mutex);
        for (const std::string& line : lines)
            PushLineLocked(line, InferConsoleLevel(line), timestampMs);
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

    // Cached filtered snapshot (stable order, oldest first). Shared so unchanged
    // frames do not deep-copy thousands of strings.
    std::shared_ptr<const std::vector<EditorConsoleEntry>> Snapshot(
        const EditorConsoleFilter& filter) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_cacheValid && m_cacheGeneration == m_generation &&
            ConsoleFilterEqual(m_cacheFilter, filter) && m_cachedSnapshot)
            return m_cachedSnapshot;

        auto built = std::make_shared<std::vector<EditorConsoleEntry>>();
        built->reserve(m_entries.size());
        for (const auto& e : m_entries) {
            if (ConsoleEntryPassesFilter(e, filter))
                built->push_back(e);
        }
        m_cacheFilter = filter;
        m_cacheGeneration = m_generation;
        m_cacheValid = true;
        m_cachedSnapshot = built;
        return m_cachedSnapshot;
    }

    std::vector<EditorConsoleEntry> SnapshotAll() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::vector<EditorConsoleEntry>(m_entries.begin(), m_entries.end());
    }

private:
    void InvalidateCacheLocked()
    {
        ++m_generation;
        m_cacheValid = false;
        m_cachedSnapshot.reset();
    }

    void PushLineLocked(const std::string& line, EditorConsoleLevel level,
                        std::int64_t timestampMs)
    {
        if (line.empty())
            return;
        if (!m_entries.empty() && m_entries.back().text == line &&
            m_entries.back().level == level) {
            m_entries.back().repeatCount += 1;
            m_entries.back().timestampMs = timestampMs;
            InvalidateCacheLocked();
            return;
        }

        EditorConsoleEntry e;
        e.id = ++m_nextId;
        e.level = level;
        e.timestampMs = timestampMs;
        e.text = line;
        e.repeatCount = 1;
        m_entries.push_back(std::move(e));
        while (m_entries.size() > m_capacity)
            m_entries.pop_front();
        InvalidateCacheLocked();
    }

    mutable std::mutex m_mutex;
    size_t m_capacity = 2000;
    std::deque<EditorConsoleEntry> m_entries;
    std::uint64_t m_generation = 0;
    std::uint64_t m_nextId = 0;

    mutable bool m_cacheValid = false;
    mutable std::uint64_t m_cacheGeneration = 0;
    mutable EditorConsoleFilter m_cacheFilter{};
    mutable std::shared_ptr<const std::vector<EditorConsoleEntry>> m_cachedSnapshot;
};

inline std::string FormatConsoleEntryLine(const EditorConsoleEntry& e)
{
    char timeBuf[32];
    const std::time_t sec = static_cast<std::time_t>(e.timestampMs / 1000);
    const int millis = static_cast<int>(e.timestampMs % 1000);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &sec);
#else
    localtime_r(&sec, &local);
#endif
    std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d.%03d",
                  local.tm_hour, local.tm_min, local.tm_sec, millis);

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
