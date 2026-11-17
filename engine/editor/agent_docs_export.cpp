// agent_docs_export.cpp
// See agent_docs_export.h. Keep behavior in sync with tools/export_agent_docs.py.

#include "agent_docs_export.h"

#include "engine/core/utf8_path.h"

#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace agent_docs {
namespace {

bool ReadFileBytes(const fs::path& path, std::string* out) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open())
        return false;
    out->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return !in.bad();
}

bool WriteFileBytes(const fs::path& path, const std::string& bytes, std::string* errorOut) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream outFile(path, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) {
        if (errorOut)
            *errorOut = "Cannot write " + PathToUtf8(path);
        return false;
    }
    outFile << bytes;
    if (!outFile.good()) {
        if (errorOut)
            *errorOut = "Failed writing " + PathToUtf8(path);
        return false;
    }
    return true;
}

std::string ReplaceAll(std::string text, const std::string& from, const std::string& to) {
    if (from.empty())
        return text;
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
    return text;
}

std::string Trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos)
        return {};
    const size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

std::string FirstLine(const std::string& s) {
    return Trim(s.substr(0, s.find('\n')));
}

bool IsHex40(const std::string& s) {
    if (s.size() != 40)
        return false;
    for (char c : s) {
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hex)
            return false;
    }
    return true;
}

// Copy every regular file under src into dst. Markdown gets schema links rewritten.
bool CopyTree(const fs::path& src, const fs::path& dst, std::string* errorOut) {
    std::error_code ec;
    for (fs::recursive_directory_iterator it(src, ec), end; it != end; it.increment(ec)) {
        if (ec) {
            if (errorOut)
                *errorOut = "Cannot list " + PathToUtf8(src) + ": " + ec.message();
            return false;
        }
        if (!it->is_regular_file(ec))
            continue;
        const fs::path rel = it->path().lexically_relative(src);
        if (rel.empty() || *rel.begin() == "..")
            continue;
        std::string bytes;
        if (!ReadFileBytes(it->path(), &bytes)) {
            if (errorOut)
                *errorOut = "Cannot read " + PathToUtf8(it->path());
            return false;
        }
        if (it->path().extension() == ".md")
            bytes = RewriteExportedMarkdown(bytes);
        if (!WriteFileBytes(dst / rel, bytes, errorOut))
            return false;
    }
    return true;
}

}  // namespace

std::string MakeManagedBlock(const std::string& body) {
    const std::string normalized = Trim(ReplaceAll(body, "\r\n", "\n"));
    return std::string(kBeginMarker) + "\n" + normalized + "\n" + kEndMarker + "\n";
}

bool UpsertManagedSection(const std::string& original, const std::string& block, std::string* out) {
    const size_t begin = original.find(kBeginMarker);
    const size_t end = original.find(kEndMarker);
    const bool hasBegin = begin != std::string::npos;
    const bool hasEnd = end != std::string::npos;
    if (hasBegin && hasEnd && end > begin) {
        std::string after = original.substr(end + std::char_traits<char>::length(kEndMarker));
        if (!after.empty() && after[0] == '\n')
            after.erase(0, 1);
        *out = original.substr(0, begin) + block + after;
        return true;
    }
    if (hasBegin || hasEnd)
        return false;
    std::string text = original;
    if (!text.empty() && text.back() != '\n')
        text += '\n';
    if (!text.empty())
        text += '\n';
    *out = text + block;
    return true;
}

std::string RewriteExportedMarkdown(const std::string& text) {
    return ReplaceAll(text, "](../schemas/", "](schemas/");
}

std::string ReadEngineCommit(const fs::path& engineRoot) {
    std::error_code ec;
    fs::path gitDir = engineRoot / ".git";
    if (fs::is_regular_file(gitDir, ec)) {
        // Worktree / submodule: ".git" file containing "gitdir: <path>".
        std::string content;
        if (!ReadFileBytes(gitDir, &content))
            return "unknown";
        const std::string line = FirstLine(content);
        const std::string prefix = "gitdir:";
        if (line.compare(0, prefix.size(), prefix) != 0)
            return "unknown";
        fs::path target = PathFromUtf8(Trim(line.substr(prefix.size())));
        gitDir = target.is_absolute() ? target : engineRoot / target;
    }

    std::string head;
    if (!ReadFileBytes(gitDir / "HEAD", &head))
        return "unknown";
    head = FirstLine(head);
    if (IsHex40(head))
        return head;
    const std::string refPrefix = "ref:";
    if (head.compare(0, refPrefix.size(), refPrefix) != 0)
        return "unknown";
    const std::string ref = Trim(head.substr(refPrefix.size()));

    // Linked worktrees keep shared refs in the common dir.
    std::vector<fs::path> dirs{ gitDir };
    std::string common;
    if (ReadFileBytes(gitDir / "commondir", &common)) {
        fs::path c = PathFromUtf8(FirstLine(common));
        dirs.push_back(c.is_absolute() ? c : gitDir / c);
    }
    for (const fs::path& dir : dirs) {
        std::string loose;
        if (ReadFileBytes(dir / PathFromUtf8(ref), &loose) && IsHex40(FirstLine(loose)))
            return FirstLine(loose);
        std::string packed;
        if (ReadFileBytes(dir / "packed-refs", &packed)) {
            std::istringstream lines(packed);
            std::string line;
            while (std::getline(lines, line)) {
                line = Trim(line);
                if (line.size() > 41 && IsHex40(line.substr(0, 40)) && line.substr(41) == ref)
                    return line.substr(0, 40);
            }
        }
    }
    return "unknown";
}

bool ExportToContentProject(const fs::path& engineRoot, const fs::path& projectRoot,
                            std::string* errorOut) {
    std::error_code ec;
    const fs::path agentSrc = engineRoot / "docs" / "agent";
    const fs::path schemasSrc = engineRoot / "docs" / "schemas";
    const fs::path sectionSrc = engineRoot / PathFromUtf8(kManagedSectionRel);
    if (!fs::is_directory(agentSrc, ec)) {
        if (errorOut)
            *errorOut = "Missing " + PathToUtf8(agentSrc);
        return false;
    }
    if (!fs::is_directory(schemasSrc, ec)) {
        if (errorOut)
            *errorOut = "Missing " + PathToUtf8(schemasSrc);
        return false;
    }
    std::string sectionBody;
    if (!ReadFileBytes(sectionSrc, &sectionBody)) {
        if (errorOut)
            *errorOut = "Missing " + PathToUtf8(sectionSrc);
        return false;
    }

    const fs::path dest = projectRoot / "docs" / "ivy-engine";
    if (!CopyTree(agentSrc, dest, errorOut))
        return false;
    if (!CopyTree(schemasSrc, dest / "schemas", errorOut))
        return false;
    if (!WriteFileBytes(dest / "ENGINE_COMMIT", ReadEngineCommit(engineRoot) + "\n", errorOut))
        return false;

    const std::string block = MakeManagedBlock(sectionBody);
    for (const char* name : { "AGENTS.md", "CLAUDE.md" }) {
        const fs::path file = projectRoot / name;
        std::string original;
        ReadFileBytes(file, &original);  // Missing file -> empty.
        std::string updated;
        if (!UpsertManagedSection(original, block, &updated)) {
            if (errorOut)
                *errorOut = "Broken managed markers in " + PathToUtf8(file);
            return false;
        }
        if (updated != original && !WriteFileBytes(file, updated, errorOut))
            return false;
    }
    return true;
}

}  // namespace agent_docs
