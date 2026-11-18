// save_store.h
// Game save data: named slots of key-value pairs, one JSON file per slot.
//
//   <dir>/<slot>.json      { "format": "ivy-save", "version": 1,
//                            "savedAt": "2026-11-18T16:00:00Z", "data": { ... } }
//   <dir>/<slot>.json.bak  the previous committed file
//
// Set* changes memory only; Commit(slot) writes the slot: a temp file first,
// then the replace step moves it over the old file and keeps the old one as
// .bak. Loading falls back to .bak when the main file is unreadable.
//
// No Windows dependency (the Windows layer, save_system.cpp, picks the folder
// and passes a MoveFileExW replace step), so it is unit-testable on Linux.
// Not thread-safe; the caller serialises access.

#pragma once

#include "libraries/nlohmann/json.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace save_store {

inline constexpr int kFormatVersion = 1;
inline constexpr const char* kFormatName = "ivy-save";
inline constexpr std::size_t kMaxSlotNameLength = 32;
inline constexpr std::size_t kMaxKeyLength = 128;
inline constexpr std::size_t kMaxFileBytes = 4u * 1024u * 1024u;

// Slot names become file names: 1-32 characters of A-Z a-z 0-9 _ -.
bool IsValidSlotName(std::string_view slot);
// Keys: 1-128 bytes, no control characters (< 0x20, 0x7F).
bool IsValidKey(std::string_view key);

// A folder name for the game's saves: keeps letters, digits, space, - _ . ( )
// and non-ASCII UTF-8 bytes, replaces other characters with '_', trims spaces
// and dots, and avoids reserved Windows names (CON, NUL, COM1, ...).
// Returns fallback when nothing usable is left.
std::string SanitizeFolderName(std::string_view name, std::string_view fallback);

// Moves tmp over target. When backup is not empty and target exists, target
// is kept as backup first. Returns false and fills error on failure.
using ReplaceFileFn = std::function<bool(const std::filesystem::path& tmp,
                                         const std::filesystem::path& target,
                                         const std::filesystem::path& backup,
                                         std::string* error)>;
using LogFn = std::function<void(const std::string& line)>;

// std::filesystem version (copy target to backup, rename tmp to target).
bool DefaultReplaceFile(const std::filesystem::path& tmp, const std::filesystem::path& target,
                        const std::filesystem::path& backup, std::string* error);

class Store {
public:
    explicit Store(std::filesystem::path directory,
                   ReplaceFileFn replace = DefaultReplaceFile,
                   LogFn log = {});

    const std::filesystem::path& Directory() const { return m_directory; }

    // Typed values. Setters return false for an invalid slot / key or a slot
    // written by a newer engine (read-only). Getters return false when the key
    // is missing or holds another type (GetInt: integers only; GetDouble: any
    // number).
    bool SetInt(std::string_view slot, std::string_view key, std::int64_t value);
    bool SetDouble(std::string_view slot, std::string_view key, double value);
    bool SetBool(std::string_view slot, std::string_view key, bool value);
    bool SetString(std::string_view slot, std::string_view key, std::string_view value);
    bool GetInt(std::string_view slot, std::string_view key, std::int64_t* out);
    bool GetDouble(std::string_view slot, std::string_view key, double* out);
    bool GetBool(std::string_view slot, std::string_view key, bool* out);
    bool GetString(std::string_view slot, std::string_view key, std::string* out);

    // Any JSON value as text (objects, arrays, numbers ...). SetJsonText
    // returns false for text that does not parse. GetJsonText returns the
    // compact text of whatever the key holds.
    bool SetJsonText(std::string_view slot, std::string_view key, std::string_view jsonText);
    bool GetJsonText(std::string_view slot, std::string_view key, std::string* out);

    bool HasKey(std::string_view slot, std::string_view key);
    bool DeleteKey(std::string_view slot, std::string_view key);

    // Writes the slot (even when unchanged). False on an invalid / read-only
    // slot, a file over kMaxFileBytes, or an I/O failure (logged).
    bool Commit(std::string_view slot);

    // On disk: a committed file or its backup exists. Uncommitted slots do not count.
    bool SlotExists(std::string_view slot) const;
    // Removes the slot's files and its in-memory data.
    bool DeleteSlot(std::string_view slot);
    // Slot names on disk, sorted.
    std::vector<std::string> ListSlots() const;

    // Forgets every loaded slot (uncommitted changes are dropped).
    void DropCache();

    std::filesystem::path MainPath(std::string_view slot) const;
    std::filesystem::path BackupPath(std::string_view slot) const;
    std::filesystem::path TempPath(std::string_view slot) const;

private:
    struct Slot {
        nlohmann::json data = nlohmann::json::object();
        bool readOnly = false;      // written by a newer format version
        bool mainCorrupt = false;   // main file unreadable: do not back it up
    };

    Slot* Load(std::string_view slot);
    bool SetValue(std::string_view slot, std::string_view key, nlohmann::json value);
    const nlohmann::json* Find(std::string_view slot, std::string_view key);
    void Log(const std::string& line) const;

    std::filesystem::path m_directory;
    ReplaceFileFn m_replace;
    LogFn m_log;
    std::map<std::string, Slot, std::less<>> m_slots;
};

}  // namespace save_store
