// package_assets.h
// Which files under a content project's rom/ a playable package needs.
//
// Starting from the default scene, follow every reference we can see:
//   - "rom/..." strings in scene / prefab / material / project.json text, and in
//     GameScripts.dll (ASCII and UTF-16 strings, so a script's literal path counts)
//   - a "rom/..." string that names a folder, or is the start of file names, keeps
//     that folder / those files (a script that builds "rom/audio/sfx_" + n)
//   - scene file names ("scene_next.json") mentioned by JSON or by the DLL
//   - texture / model / effect file names stored inside FBX / OBJ / MTL / Effekseer files
// Anything not reached is left out. project.json build.alwaysInclude adds entries
// that code builds at runtime; build.packAllAssets bypasses all of this.
//
// Pure logic (std::filesystem only), unit-testable on Linux.

#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace package_assets {

struct Options {
    std::filesystem::path romRoot;             // <content project>/rom
    std::vector<std::string> rootScenes;       // file names below rom/scene (default scene)
    std::vector<std::string> alwaysInclude;    // "rom/..." file, folder or name prefix
    std::vector<std::filesystem::path> scanFiles;  // outside rom/: project.json, GameScripts.dll
};

struct Result {
    std::set<std::string> included;     // rom-relative, '/' separators, original case
    std::vector<std::string> excluded;  // rom-relative files nothing references
    std::vector<std::string> warnings;
    unsigned long long includedBytes = 0;
    unsigned long long excludedBytes = 0;
};

Result ComputeReferencedAssets(const Options& options);

// Exposed for tests. Strings of 5+ characters found in a binary (ASCII / UTF-8 runs and
// UTF-16LE runs), one per line.
std::string ExtractStrings(const std::string& bytes);

// "rom/..." tokens in text, lower case, normalized to '/' separators. `strictStart` rejects a
// token glued to a word ("from/x"); strings pulled out of binaries turn it off because
// neighbouring junk bytes can sit right in front of a real string.
std::vector<std::string> CollectRomTokens(const std::string& text, bool strictStart = true);

}  // namespace package_assets
