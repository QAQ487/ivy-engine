// scene_file_monitor.h
// Declares SceneFileMonitor.

#pragma once
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

// Main-thread polling only. Observing/acknowledging a version never loads a scene.
class SceneFileMonitor {
public:
    struct Version { std::string path, content; };

    void Reset() { *this = SceneFileMonitor{}; }
    void SetBaseline(const std::string& path, const std::string& content) {
        Reset();
        m_path = path;
        m_handled = content;
        m_haveStamp = Stat(m_stamp);
        // The disk may already have changed since the successful load/save.
        m_pending = true;
    }
    const std::string& Path() const { return m_path; }
    bool HasPendingChange() const { return m_pending; }
    static bool Read(const std::string& path, std::string& content) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        std::ostringstream buffer;
        buffer << file.rdbuf();
        if (file.bad()) return false;
        content = buffer.str();
        return true;
    }
    std::optional<Version> Poll(double elapsed) {
        m_clock += elapsed;
        if (m_path.empty() || m_clock < m_nextPoll) return {};
        m_nextPoll = m_clock + 0.5;
        Stamp stamp;
        if (!Stat(stamp)) { m_haveStamp = false; return {}; }
        if (!m_haveStamp || !(stamp == m_stamp)) {
            m_stamp = stamp;
            m_haveStamp = true;
            m_changedAt = m_clock;
            m_pending = true;
            return {};
        }
        if (!m_pending || m_clock - m_changedAt < 0.5) return {};
        std::string content;
        if (!Read(m_path, content)) return {};
        Stamp after;
        if (!Stat(after) || !(after == stamp)) { m_haveStamp = false; return {}; }
        m_pending = false;
        if (content == m_handled) return {};
        // Includes ignored/failed versions: retry only after content changes.
        m_handled = content;
        return Version{m_path, std::move(content)};
    }
private:
    struct Stamp {
        std::filesystem::file_time_type time{};
        std::uintmax_t size = 0;
        bool operator==(const Stamp& other) const { return time == other.time && size == other.size; }
    };
    bool Stat(Stamp& stamp) const {
        std::error_code ec;
        stamp.time = std::filesystem::last_write_time(m_path, ec);
        if (ec) return false;
        stamp.size = std::filesystem::file_size(m_path, ec);
        return !ec;
    }
    std::string m_path, m_handled;
    Stamp m_stamp;
    bool m_haveStamp = false, m_pending = false;
    double m_clock = 0, m_nextPoll = 0, m_changedAt = 0;
};
