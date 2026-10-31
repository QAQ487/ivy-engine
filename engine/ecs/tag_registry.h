// tag_registry.h
// Declares Tag.

#ifndef TAG_REGISTRY_H
#define TAG_REGISTRY_H

#include <string>
#include <vector>

// Project-wide list of GameObject tags. Built-in names keep stable indices that
// match enum class Tag (TAG_NONE=0 …). Custom tags are appended and persisted.
class TagRegistry {
public:
    static TagRegistry& Instance();

    void EnsureDefaults();
    bool LoadFromFile(const std::string& path = "rom/tags.json");
    bool SaveToFile(const std::string& path = "rom/tags.json") const;

    int GetCount() const { return static_cast<int>(m_names.size()); }
    const std::vector<std::string>& GetNames() const { return m_names; }
    const char* GetName(int id) const;
    bool IsValid(int id) const;
    int FindByName(const std::string& name) const;

    // Appends a new tag. Returns its id, or -1 if name is empty/duplicate.
    int AddTag(const std::string& name);

    // Built-in tags (indices < Tag::TAG_MAX) cannot be removed.
    bool IsBuiltin(int id) const;
    bool CanRemove(int id) const;

    // Removes a custom tag. Returns false if id is invalid or built-in.
    // Callers must remap any GameObject tag ids afterward.
    bool RemoveTag(int id);

private:
    TagRegistry();

    std::vector<std::string> m_names;
};

#endif // TAG_REGISTRY_H
