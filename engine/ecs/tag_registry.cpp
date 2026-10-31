// tag_registry.cpp
// Implements Tag.

#include "tag_registry.h"
#include "game_object.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <locale>

#include "libraries/nlohmann/json.hpp"

using json = nlohmann::json;

namespace {
    const char* kDefaultTagNames[] = {
        "NONE",
        "SHADOW",
        "GROUND",
        "OBSTACLES",
        "WALL",
        "PLAYER",
        "ENEMY",
        "BULLET",
        "SCLAP",
    };
    static_assert(
        sizeof(kDefaultTagNames) / sizeof(kDefaultTagNames[0]) ==
            static_cast<size_t>(Tag::TAG_MAX),
        "Default tag names must match enum class Tag order and count");

    std::string TrimCopy(const std::string& value) {
        const char* whitespace = " \t\r\n";
        const size_t start = value.find_first_not_of(whitespace);
        if (start == std::string::npos)
            return {};
        const size_t end = value.find_last_not_of(whitespace);
        return value.substr(start, end - start + 1);
    }

    std::string ToUpperCopy(const std::string& value) {
        std::string result = value;
        const std::locale loc = std::locale::classic();
        std::transform(result.begin(), result.end(), result.begin(),
            [loc](unsigned char c) {
                return static_cast<char>(std::toupper(c, loc));
            });
        return result;
    }
}

TagRegistry& TagRegistry::Instance() {
    static TagRegistry instance;
    return instance;
}

TagRegistry::TagRegistry() {
    EnsureDefaults();
}

void TagRegistry::EnsureDefaults() {
    m_names.clear();
    m_names.reserve(static_cast<size_t>(Tag::TAG_MAX));
    for (const char* name : kDefaultTagNames)
        m_names.emplace_back(name);
}

const char* TagRegistry::GetName(int id) const {
    if (!IsValid(id))
        return "NONE";
    return m_names[static_cast<size_t>(id)].c_str();
}

bool TagRegistry::IsValid(int id) const {
    return id >= 0 && id < GetCount();
}

int TagRegistry::FindByName(const std::string& name) const {
    const std::string needle = ToUpperCopy(TrimCopy(name));
    if (needle.empty())
        return -1;
    for (int i = 0; i < GetCount(); ++i) {
        if (ToUpperCopy(m_names[static_cast<size_t>(i)]) == needle)
            return i;
    }
    return -1;
}

int TagRegistry::AddTag(const std::string& name) {
    const std::string trimmed = TrimCopy(name);
    if (trimmed.empty())
        return -1;
    if (FindByName(trimmed) >= 0)
        return -1;

    m_names.push_back(trimmed);
    return GetCount() - 1;
}

bool TagRegistry::IsBuiltin(int id) const {
    return id >= 0 && id < static_cast<int>(Tag::TAG_MAX);
}

bool TagRegistry::CanRemove(int id) const {
    return IsValid(id) && !IsBuiltin(id);
}

bool TagRegistry::RemoveTag(int id) {
    if (!CanRemove(id))
        return false;
    m_names.erase(m_names.begin() + id);
    return true;
}

bool TagRegistry::LoadFromFile(const std::string& path) {
    EnsureDefaults();

    std::ifstream in(path);
    if (!in.is_open())
        return false;

    json root;
    try {
        in >> root;
    } catch (...) {
        return false;
    }

    if (!root.contains("tags") || !root["tags"].is_array())
        return false;

    // Keep built-in slots stable; replace names only if the file provides them,
    // then append any extra custom tags after TAG_MAX.
    const auto& tags = root["tags"];
    for (size_t i = 0; i < tags.size(); ++i) {
        if (!tags[i].is_string())
            continue;
        const std::string name = TrimCopy(tags[i].get<std::string>());
        if (name.empty())
            continue;

        if (i < static_cast<size_t>(Tag::TAG_MAX)) {
            m_names[i] = name;
        } else if (FindByName(name) < 0) {
            m_names.push_back(name);
        }
    }
    return true;
}

bool TagRegistry::SaveToFile(const std::string& path) const {
    json root;
    root["tags"] = m_names;

    std::ofstream out(path);
    if (!out.is_open())
        return false;
    out << root.dump(2);
    return true;
}
