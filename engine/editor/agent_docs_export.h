// agent_docs_export.h
// Copy the engine's game-agent doc pack into a content project (File > New Project).
//
// Mirrors tools/export_agent_docs.py for a fresh project:
//   <engine>/docs/agent/**   -> <project>/docs/ivy-engine/**   (schema links rewritten)
//   <engine>/docs/schemas/** -> <project>/docs/ivy-engine/schemas/**
//   engine commit            -> <project>/docs/ivy-engine/ENGINE_COMMIT
//   managed section (incl. engine-boundary rules, text from
//   <engine>/tools/agent_docs_managed_section.md) upserted into
//   <project>/AGENTS.md and <project>/CLAUDE.md.
// Re-exports after engine updates use the Python tool, which also prunes stale files.
//
// Pure std::filesystem; no Windows headers, so Linux unit tests can compile it.
#pragma once

#include <filesystem>
#include <string>

namespace agent_docs {

inline constexpr const char* kBeginMarker = "<!-- ivy-engine-docs:begin -->";
inline constexpr const char* kEndMarker = "<!-- ivy-engine-docs:end -->";
inline constexpr const char* kManagedSectionRel = "tools/agent_docs_managed_section.md";

// Wrap the shared section body in begin/end markers (CRLF normalized, trimmed, LF-terminated).
std::string MakeManagedBlock(const std::string& body);

// Replace an existing marker block in place, or append the block (blank line before it).
// Returns false (and leaves *out untouched) when only one of the two markers is present.
bool UpsertManagedSection(const std::string& original, const std::string& block, std::string* out);

// "](../schemas/" -> "](schemas/" so links work under docs/ivy-engine/.
std::string RewriteExportedMarkdown(const std::string& text);

// HEAD commit hash read from <engineRoot>/.git without running git
// (detached HEAD, loose ref, packed-refs, or a worktree .git file). "unknown" if unreadable.
std::string ReadEngineCommit(const std::filesystem::path& engineRoot);

// Export into projectRoot. Returns false with *errorOut on failure; the caller decides
// whether that is fatal (New Project treats it as a warning).
bool ExportToContentProject(const std::filesystem::path& engineRoot,
                            const std::filesystem::path& projectRoot,
                            std::string* errorOut);

}  // namespace agent_docs
