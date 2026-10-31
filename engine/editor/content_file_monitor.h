// content_file_monitor.h
// Watches content files for editor hot-reload.

#pragma once
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <future>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <array>
#include <set>
#include <cstddef>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

// Content fingerprints also detect replacements that preserve size and write time.
#ifdef _MSC_VER
#pragma optimize("gt", on)
#endif
inline unsigned long long ContentHashFile(std::istream& stream) {
    unsigned long long hash = 14695981039346656037ull;
    char data[65536];
    while (stream) {
        stream.read(data, sizeof(data));
        const auto count = stream.gcount();
        for (std::streamsize n = 0; n < count; ++n) {
            hash ^= static_cast<unsigned char>(data[n]); hash *= 1099511628211ull;
        }
    }
    return hash;
}
#ifdef _MSC_VER
#pragma optimize("", on)
#endif

// Worker only observes files. Consumers apply batches at a main-thread frame boundary.
class ContentFileMonitor {
public:
    struct File { std::uintmax_t size = 0; std::filesystem::file_time_type time{}; unsigned long long hash = 0;
        bool operator==(const File& b) const { return size == b.size && hash == b.hash; } };
    using Snapshot = std::map<std::string, File>;
    struct Change { std::string path; bool added = false, removed = false; };
    static std::string Utf8Path(const std::filesystem::path& path) {
        const auto bytes = path.generic_u8string();
        return std::string(bytes.begin(), bytes.end());
    }
    ~ContentFileMonitor() { StopWatch(); }
    static Snapshot Scan(const std::filesystem::path& root, const Snapshot& previous = {}, bool audit = true, const std::set<std::string>& dirty = {}) {
        Snapshot result;
        std::error_code ec;
        if (!std::filesystem::exists(root, ec)) return result;
        for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end;
             it != end && !ec; it.increment(ec)) {
            auto relative = Utf8Path(it->path().lexically_relative(root));
            const auto name = Utf8Path(it->path().filename());
            if (name.empty() || name[0] == '.' || relative == "shader" || relative.rfind("shader/", 0) == 0) {
                if (it->is_directory(ec)) it.disable_recursion_pending();
                continue;
            }
            if (it->is_directory(ec)) { result.emplace(relative + "/", File{}); continue; }
            if (!it->is_regular_file(ec)) continue;
            auto ext = it->path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            if (ext == ".tmp" || ext == ".bak" || (ext == ".obj" && relative.rfind("model/", 0) != 0) ||
                ext == ".pdb" || ext == ".ilk" || ext == ".log" || name.find(".hot_") != std::string::npos || name.back() == '~') continue;
            File stamp;
            stamp.time = it->last_write_time(ec); if (ec) break;
            stamp.size = it->file_size(ec); if (ec) break;
            const auto old = previous.find(relative);
            if (!audit && !dirty.count(relative) && old != previous.end() && old->second.time == stamp.time && old->second.size == stamp.size) {
                result.emplace(relative, old->second); continue;
            }
            std::ifstream stream(it->path(), std::ios::binary); if (!stream) throw std::runtime_error("Content file is busy: " + relative);
            const auto hash = ContentHashFile(stream);
            if (stream.bad()) throw std::runtime_error("Cannot read content: " + relative);
            if (std::filesystem::last_write_time(it->path(), ec) != stamp.time || ec || std::filesystem::file_size(it->path(), ec) != stamp.size || ec) throw std::runtime_error("Content changed during read: " + relative);
            stamp.hash = hash;
            result.emplace(relative, stamp);
        }
        // An incomplete traversal must never manufacture a batch of deletions.
        if (ec) throw std::runtime_error("Cannot scan content directory: " + ec.message());
        return result;
    }
    std::vector<Change> Observe(Snapshot next, double now) {
        if (!m_initialized) { m_baseline = next; m_candidate = std::move(next); m_initialized = true; return {}; }
        if (next != m_candidate) { m_candidate = std::move(next); m_changedAt = now; return {}; }
        if (now - m_changedAt < 0.5 || m_candidate == m_baseline) return {};
        std::vector<Change> changes;
        for (const auto& [path, file] : m_candidate) { const auto old = m_baseline.find(path);
            if (old == m_baseline.end() || !(old->second == file)) changes.push_back({path, old == m_baseline.end(), false}); }
        for (const auto& [path, file] : m_baseline) if (!m_candidate.count(path)) changes.push_back({path, false, true});
        m_baseline = m_candidate;
        return changes;
    }
    std::vector<Change> Poll(const std::filesystem::path& root) {
        const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        if (m_root != root) {
            // Consume the previous project's worker before resetting; no worker uses cwd.
            if (m_worker.valid()) m_worker.wait();
            StopWatch();
#ifdef _WIN32
            m_directory = CreateFileW(root.c_str(), FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
            if (m_directory != INVALID_HANDLE_VALUE) {
                m_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                m_overlapped = {}; m_overlapped.hEvent = m_event;
                if (m_event) IssueWatch();
            }
#endif
            m_dirty.clear();
            m_scanCache.clear(); m_nextAudit = 0;
            m_worker = {}; m_root = root; m_initialized = false; m_baseline.clear(); m_candidate.clear(); m_nextPoll = 0;
        }
#ifdef _WIN32
        if (m_watchPending && WaitForSingleObject(m_event, 0) == WAIT_OBJECT_0) {
            DWORD bytes = 0;
            if (!GetOverlappedResult(m_directory, &m_overlapped, &bytes, FALSE) || bytes == 0) m_forceAudit = true;
            else {
                size_t offset = 0;
                while (offset + offsetof(FILE_NOTIFY_INFORMATION, FileName) <= bytes) {
                    const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(m_notifications.data() + offset);
                    if (offset + offsetof(FILE_NOTIFY_INFORMATION, FileName) + info->FileNameLength > bytes) { m_forceAudit = true; break; }
                    const std::wstring name(info->FileName, info->FileNameLength / sizeof(wchar_t));
                    m_dirty.insert(Utf8Path(std::filesystem::path(name)));
                    if (!info->NextEntryOffset) break;
                    if (info->NextEntryOffset > bytes - offset) { m_forceAudit = true; break; }
                    offset += info->NextEntryOffset;
                }
            }
            m_watchPending = false;
            ResetEvent(m_event); IssueWatch();
            // A full fingerprint audit is an idle fallback, not work for every edit.
            m_nextAudit = now + 10.0;
        }
#endif
        if (m_worker.valid()) {
            if (m_worker.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return {};
            try { m_scanCache = m_worker.get(); m_scanningDirty.clear(); auto changes = Observe(m_scanCache, now); m_error.clear(); return changes; }
            catch (const std::exception& e) { m_dirty.insert(m_scanningDirty.begin(), m_scanningDirty.end()); m_scanningDirty.clear(); m_forceAudit = true; m_error = e.what(); return {}; }
        }
        if (now >= m_nextPoll) {
            m_nextPoll = now + 0.25;
            const bool audit = m_forceAudit || now >= m_nextAudit;
            if (audit) {
#ifdef _WIN32
                m_nextAudit = now + (!m_watchPending ? 1.0 : 10.0);
#else
                m_nextAudit = now + 1.0;
#endif
            }
            m_forceAudit = false;
            m_scanningDirty = std::move(m_dirty); m_dirty.clear();
            const auto dirty = m_scanningDirty;
            m_worker = std::async(std::launch::async, [root, previous = m_scanCache, audit, dirty] { return Scan(root, previous, audit, dirty); });
        }
        return {};
    }
    void Stop() {
        if (m_worker.valid()) m_worker.wait();
        m_worker = {};
        StopWatch(); m_dirty.clear();
        m_root.clear(); m_scanCache.clear(); m_baseline.clear(); m_candidate.clear(); m_initialized = false;
        m_error.clear(); m_nextPoll = 0; m_nextAudit = 0;
    }
    void RefreshSoon() { m_nextPoll = 0; m_forceAudit = true; }
    bool Pending() const { return m_initialized && m_candidate != m_baseline; }
    const std::string& Error() const { return m_error; }
private:
    void StopWatch() {
#ifdef _WIN32
        if (m_directory != INVALID_HANDLE_VALUE) {
            CancelIoEx(m_directory, &m_overlapped);
            if (m_watchPending) { DWORD ignored = 0; GetOverlappedResult(m_directory, &m_overlapped, &ignored, TRUE); }
            CloseHandle(m_directory); m_directory = INVALID_HANDLE_VALUE;
        }
        if (m_event) { CloseHandle(m_event); m_event = nullptr; }
        m_watchPending = false;
#endif
    }
#ifdef _WIN32
    void IssueWatch() {
        m_overlapped = {}; m_overlapped.hEvent = m_event;
        m_watchPending = ReadDirectoryChangesW(m_directory, m_notifications.data(), static_cast<DWORD>(m_notifications.size()), TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
            nullptr, &m_overlapped, nullptr) != FALSE;
    }
    HANDLE m_directory = INVALID_HANDLE_VALUE, m_event = nullptr;
    OVERLAPPED m_overlapped{};
    alignas(DWORD) std::array<unsigned char, 65536> m_notifications{};
    bool m_watchPending = false;
#endif
    std::set<std::string> m_dirty, m_scanningDirty;
    Snapshot m_scanCache;
    bool m_forceAudit = false;
    double m_nextAudit = 0;
    Snapshot m_baseline, m_candidate;
    std::filesystem::path m_root;
    std::future<Snapshot> m_worker;
    bool m_initialized = false;
    double m_changedAt = 0, m_nextPoll = 0;
    std::string m_error;
};
