// save_store.cpp
// See save_store.h.

#include "engine/save/save_store.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iterator>
#include <system_error>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace save_store {

namespace {

bool IsSlotChar(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '-';
}

std::string UtcTimestamp() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

std::string PathText(const fs::path& p) {
    const auto u8 = p.u8string();
    return std::string(u8.begin(), u8.end());
}

// Reads and validates one save file. Returns false when it is missing,
// too large, not JSON, or not an ivy-save document.
bool ReadSaveFile(const fs::path& path, json* outData, int* outVersion, std::string* why) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) {
        *why = "missing";
        return false;
    }
    const auto size = fs::file_size(path, ec);
    if (ec || size > kMaxFileBytes) {
        *why = ec ? "unreadable size" : "larger than 4 MiB";
        return false;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        *why = "cannot open";
        return false;
    }
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    json doc = json::parse(text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        *why = "not valid JSON";
        return false;
    }
    const auto format = doc.find("format");
    const auto version = doc.find("version");
    const auto data = doc.find("data");
    if (format == doc.end() || !format->is_string() || format->get<std::string>() != kFormatName ||
        version == doc.end() || !version->is_number_integer() || data == doc.end() ||
        !data->is_object()) {
        *why = "not an ivy-save file";
        return false;
    }
    *outVersion = version->get<int>();
    *outData = std::move(*data);
    return true;
}

}  // namespace

bool IsValidSlotName(std::string_view slot) {
    if (slot.empty() || slot.size() > kMaxSlotNameLength)
        return false;
    return std::all_of(slot.begin(), slot.end(), [](char c) { return IsSlotChar(static_cast<unsigned char>(c)); });
}

bool IsValidKey(std::string_view key) {
    if (key.empty() || key.size() > kMaxKeyLength)
        return false;
    return std::none_of(key.begin(), key.end(), [](char c) {
        const unsigned char u = static_cast<unsigned char>(c);
        return u < 0x20 || u == 0x7F;
    });
}

std::string SanitizeFolderName(std::string_view name, std::string_view fallback) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        const unsigned char u = static_cast<unsigned char>(c);
        const bool keep = u >= 0x80 || std::isalnum(u) || c == ' ' || c == '-' || c == '_' ||
                          c == '.' || c == '(' || c == ')';
        out.push_back(keep ? c : '_');
    }
    const auto first = out.find_first_not_of(" .");
    if (first == std::string::npos)
        return std::string(fallback);
    const auto last = out.find_last_not_of(" .");
    out = out.substr(first, last - first + 1);
    if (out.size() > 64)
        out.resize(64);

    // Reserved device names, with or without an extension.
    std::string stem = out.substr(0, out.find('.'));
    for (char& c : stem)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    static const char* const kReserved[] = {"CON", "PRN", "AUX", "NUL",
                                            "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                                            "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
    for (const char* r : kReserved) {
        if (stem == r)
            return "_" + out;
    }
    return out;
}

bool DefaultReplaceFile(const fs::path& tmp, const fs::path& target, const fs::path& backup,
                        std::string* error) {
    std::error_code ec;
    if (!backup.empty() && fs::exists(target, ec)) {
        fs::copy_file(target, backup, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            if (error) *error = "backup copy failed: " + ec.message();
            return false;
        }
    }
    fs::rename(tmp, target, ec);
    if (ec) {
        if (error) *error = "rename failed: " + ec.message();
        return false;
    }
    return true;
}

Store::Store(fs::path directory, ReplaceFileFn replace, LogFn log)
    : m_directory(std::move(directory)),
      m_replace(replace ? std::move(replace) : ReplaceFileFn(DefaultReplaceFile)),
      m_log(std::move(log)) {}

fs::path Store::MainPath(std::string_view slot) const {
    return m_directory / (std::string(slot) + ".json");
}
fs::path Store::BackupPath(std::string_view slot) const {
    return m_directory / (std::string(slot) + ".json.bak");
}
fs::path Store::TempPath(std::string_view slot) const {
    return m_directory / (std::string(slot) + ".json.tmp");
}

void Store::Log(const std::string& line) const {
    if (m_log)
        m_log(line);
}

Store::Slot* Store::Load(std::string_view slot) {
    if (!IsValidSlotName(slot))
        return nullptr;
    if (auto it = m_slots.find(slot); it != m_slots.end())
        return &it->second;

    Slot loaded;
    json data;
    int version = 0;
    std::string whyMain, whyBackup;
    bool ok = ReadSaveFile(MainPath(slot), &data, &version, &whyMain);
    if (!ok) {
        std::error_code ec;
        loaded.mainCorrupt = fs::exists(MainPath(slot), ec);
        ok = ReadSaveFile(BackupPath(slot), &data, &version, &whyBackup);
        if (ok)
            Log("[Save] slot '" + std::string(slot) + "': main file " + whyMain +
                "; loaded the backup " + PathText(BackupPath(slot)));
        else if (loaded.mainCorrupt || whyBackup != "missing")
            Log("[Save] slot '" + std::string(slot) + "': main file " + whyMain + ", backup " +
                whyBackup + "; starting empty");
    }
    if (ok) {
        loaded.data = std::move(data);
        if (version > kFormatVersion) {
            loaded.readOnly = true;
            Log("[Save] slot '" + std::string(slot) + "' was written by a newer engine (format " +
                std::to_string(version) + "); it can be read but not changed");
        }
    }
    return &m_slots.emplace(std::string(slot), std::move(loaded)).first->second;
}

bool Store::SetValue(std::string_view slot, std::string_view key, json value) {
    if (!IsValidKey(key))
        return false;
    Slot* s = Load(slot);
    if (!s || s->readOnly)
        return false;
    s->data[std::string(key)] = std::move(value);
    return true;
}

const json* Store::Find(std::string_view slot, std::string_view key) {
    if (!IsValidKey(key))
        return nullptr;
    Slot* s = Load(slot);
    if (!s)
        return nullptr;
    const auto it = s->data.find(std::string(key));
    return it == s->data.end() ? nullptr : &*it;
}

bool Store::SetInt(std::string_view slot, std::string_view key, std::int64_t value) {
    return SetValue(slot, key, json(value));
}
bool Store::SetDouble(std::string_view slot, std::string_view key, double value) {
    return SetValue(slot, key, json(value));
}
bool Store::SetBool(std::string_view slot, std::string_view key, bool value) {
    return SetValue(slot, key, json(value));
}
bool Store::SetString(std::string_view slot, std::string_view key, std::string_view value) {
    return SetValue(slot, key, json(std::string(value)));
}

bool Store::GetInt(std::string_view slot, std::string_view key, std::int64_t* out) {
    const json* v = Find(slot, key);
    if (!v || !out)
        return false;
    if (v->is_number_unsigned()) {
        const auto u = v->get<std::uint64_t>();
        if (u > static_cast<std::uint64_t>(INT64_MAX))
            return false;
        *out = static_cast<std::int64_t>(u);
        return true;
    }
    if (!v->is_number_integer())
        return false;
    *out = v->get<std::int64_t>();
    return true;
}

bool Store::GetDouble(std::string_view slot, std::string_view key, double* out) {
    const json* v = Find(slot, key);
    if (!v || !out || !v->is_number())
        return false;
    *out = v->get<double>();
    return true;
}

bool Store::GetBool(std::string_view slot, std::string_view key, bool* out) {
    const json* v = Find(slot, key);
    if (!v || !out || !v->is_boolean())
        return false;
    *out = v->get<bool>();
    return true;
}

bool Store::GetString(std::string_view slot, std::string_view key, std::string* out) {
    const json* v = Find(slot, key);
    if (!v || !out || !v->is_string())
        return false;
    *out = v->get<std::string>();
    return true;
}

bool Store::SetJsonText(std::string_view slot, std::string_view key, std::string_view jsonText) {
    json value = json::parse(jsonText.begin(), jsonText.end(), nullptr, false);
    if (value.is_discarded())
        return false;
    return SetValue(slot, key, std::move(value));
}

bool Store::GetJsonText(std::string_view slot, std::string_view key, std::string* out) {
    const json* v = Find(slot, key);
    if (!v || !out)
        return false;
    *out = v->dump(-1, ' ', false, json::error_handler_t::replace);
    return true;
}

bool Store::HasKey(std::string_view slot, std::string_view key) {
    return Find(slot, key) != nullptr;
}

bool Store::DeleteKey(std::string_view slot, std::string_view key) {
    if (!IsValidKey(key))
        return false;
    Slot* s = Load(slot);
    if (!s || s->readOnly)
        return false;
    return s->data.erase(std::string(key)) > 0;
}

bool Store::Commit(std::string_view slot) {
    Slot* s = Load(slot);
    if (!s)
        return false;
    if (s->readOnly) {
        Log("[Save] slot '" + std::string(slot) + "' is read-only (newer format); not saved");
        return false;
    }
    json doc = json::object();
    doc["format"] = kFormatName;
    doc["version"] = kFormatVersion;
    doc["savedAt"] = UtcTimestamp();
    doc["data"] = s->data;
    const std::string text = doc.dump(2, ' ', false, json::error_handler_t::replace) + "\n";
    if (text.size() > kMaxFileBytes) {
        Log("[Save] slot '" + std::string(slot) + "' is larger than 4 MiB; not saved");
        return false;
    }

    std::error_code ec;
    fs::create_directories(m_directory, ec);
    if (ec) {
        Log("[Save] cannot create " + PathText(m_directory) + ": " + ec.message());
        return false;
    }
    const fs::path tmp = TempPath(slot);
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.flush();
        if (!out) {
            out.close();
            fs::remove(tmp, ec);
            Log("[Save] cannot write " + PathText(tmp));
            return false;
        }
    }
    // An unreadable main file is not worth keeping: leave the good backup alone.
    const fs::path backup = s->mainCorrupt ? fs::path() : BackupPath(slot);
    std::string error;
    if (!m_replace(tmp, MainPath(slot), backup, &error)) {
        fs::remove(tmp, ec);
        Log("[Save] cannot save slot '" + std::string(slot) + "': " + error);
        return false;
    }
    s->mainCorrupt = false;
    return true;
}

bool Store::SlotExists(std::string_view slot) const {
    if (!IsValidSlotName(slot))
        return false;
    std::error_code ec;
    return fs::is_regular_file(MainPath(slot), ec) || fs::is_regular_file(BackupPath(slot), ec);
}

bool Store::DeleteSlot(std::string_view slot) {
    if (!IsValidSlotName(slot))
        return false;
    if (auto it = m_slots.find(slot); it != m_slots.end())
        m_slots.erase(it);
    std::error_code ec;
    bool ok = true;
    for (const fs::path& p : {MainPath(slot), BackupPath(slot), TempPath(slot)}) {
        fs::remove(p, ec);
        if (ec) {
            ok = false;
            Log("[Save] cannot delete " + PathText(p) + ": " + ec.message());
        }
    }
    return ok;
}

std::vector<std::string> Store::ListSlots() const {
    std::vector<std::string> names;
    std::error_code ec;
    for (fs::directory_iterator it(m_directory, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code fileEc;
        if (!it->is_regular_file(fileEc))
            continue;
        const auto u8 = it->path().filename().u8string();
        std::string file(u8.begin(), u8.end());
        for (const char* suffix : {".json.bak", ".json"}) {
            const std::string s(suffix);
            if (file.size() > s.size() && file.compare(file.size() - s.size(), s.size(), s) == 0) {
                const std::string stem = file.substr(0, file.size() - s.size());
                if (IsValidSlotName(stem))
                    names.push_back(stem);
                break;
            }
        }
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}

void Store::DropCache() {
    m_slots.clear();
}

}  // namespace save_store
