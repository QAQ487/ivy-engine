// project_file_merge.h
// Pure logic for writing project.json without losing fields the editor does
// not manage (for example save.folderName, or keys a newer engine added).
// The editor builds a JSON merge patch (RFC 7386) of the fields it owns and
// applies it to the file as it is on disk: objects merge key by key, other
// values replace, and null removes a key.
// No Windows dependency, so it is unit-testable on Linux.

#pragma once

#include "libraries/nlohmann/json.hpp"

#include <string_view>

namespace project_file {

// existingText: the current project.json (empty or unparsable -> start from {}).
// patch: the editor-managed fields; null values delete keys.
// Objects left empty by a deletion (e.g. "build" after both its keys were
// removed) are dropped so the file does not collect empty sections.
inline nlohmann::json MergeIntoExisting(std::string_view existingText, const nlohmann::json& patch) {
    nlohmann::json doc = nlohmann::json::parse(existingText.begin(), existingText.end(), nullptr, false);
    if (doc.is_discarded() || !doc.is_object())
        doc = nlohmann::json::object();
    doc.merge_patch(patch);
    if (patch.is_object()) {
        for (auto it = patch.begin(); it != patch.end(); ++it) {
            if (!it.value().is_object())
                continue;
            const auto found = doc.find(it.key());
            if (found != doc.end() && found->is_object() && found->empty())
                doc.erase(found);
        }
    }
    return doc;
}

}  // namespace project_file
