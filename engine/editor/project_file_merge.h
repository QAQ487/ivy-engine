// project_file_merge.h
// Pure logic for writing project.json without losing fields the editor does
// not manage (for example save.folderName, or keys a newer engine added).
// The editor builds a JSON merge patch (RFC 7386) of the fields it owns and
// applies it to the file as it is on disk: objects merge key by key, other
// values replace, and null removes a key.
// No Windows dependency, so it is unit-testable on Linux.

#pragma once

#include "libraries/nlohmann/json.hpp"

#include <string>
#include <string_view>

namespace project_file {

struct MergeResult {
    bool ok = false;          // false: the file on disk must not be overwritten
    nlohmann::json document;  // what to write when ok
    std::string error;        // why not, e.g. "parse error at line 9, column 1: ..."
};

// existingText: the current project.json. Empty or whitespace-only text (no
// file yet) starts from {}. Text that is not a JSON object (a hand edit with a
// missing comma, say) is refused instead of replaced, so nothing in it is lost.
// patch: the editor-managed fields; null values delete keys. Objects left empty
// by a deletion are dropped so the file does not collect empty sections.
inline MergeResult Merge(std::string_view existingText, const nlohmann::json& patch) {
    MergeResult result;
    nlohmann::json doc = nlohmann::json::object();
    if (existingText.find_first_not_of(" \t\r\n\xEF\xBB\xBF") != std::string_view::npos) {
        try {
            doc = nlohmann::json::parse(existingText.begin(), existingText.end());
        } catch (const nlohmann::json::parse_error& e) {
            result.error = e.what();
            return result;
        }
        if (!doc.is_object()) {
            result.error = "the file is not a JSON object";
            return result;
        }
    }
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
    result.ok = true;
    result.document = std::move(doc);
    return result;
}

}  // namespace project_file
