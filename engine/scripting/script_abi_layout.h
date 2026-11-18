// script_abi_layout.h
// Pure logic for the GameScripts.dll layout check. Scripts call inline
// GameObject / Rigidbody / component members and pass STL types across the
// DLL boundary, so the DLL and the host must agree on those class layouts.
// ScriptRuntimeApi's version only covers the function table.
//
// Both sides fill the same entry list (script_abi_fill.h): a manual revision,
// the build flavour (Debug / Release STL), and sizeof of the shared types.
// The host compares the two lists before it creates any script.
// No Windows or engine dependency, so it is unit-testable on Linux.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace script_abi {

// Bump when a shared class changes in a way sizeof cannot see (member order,
// virtual functions added or reordered, a member type swapped for one of the
// same size). Rebuilding GameScripts against the new headers fixes a mismatch.
inline constexpr std::uint32_t kAbiRevision = 1;

// Entry indices. Append only: never reorder or reuse an index.
enum Entry : std::uint32_t {
    kRevision = 0,
    kIteratorDebugLevel,  // _ITERATOR_DEBUG_LEVEL (MSVC): 0 Release, 2 Debug
    kDebugCrt,            // 1 when _DEBUG is defined
    kPointerSize,
    kSizeofString,
    kSizeofVector,
    kSizeofUnorderedMap,
    kSizeofFunction,
    kSizeofSharedPtr,
    kSizeofJson,
    kSizeofGameObject,
    kSizeofComponent,
    kSizeofScript,
    kSizeofRigidbody,
    kSizeofTransform,
    kSizeofCollider,
    kSizeofRigidbodyComponent,
    kEntryCount
};

inline const char* EntryName(std::uint32_t index) {
    static const char* const kNames[kEntryCount] = {
        "ABI revision",
        "_ITERATOR_DEBUG_LEVEL",
        "_DEBUG",
        "pointer size",
        "sizeof(std::string)",
        "sizeof(std::vector)",
        "sizeof(std::unordered_map)",
        "sizeof(std::function)",
        "sizeof(std::shared_ptr)",
        "sizeof(nlohmann::json)",
        "sizeof(GameObject)",
        "sizeof(Component)",
        "sizeof(Script)",
        "sizeof(Rigidbody)",
        "sizeof(engine::Transform)",
        "sizeof(Collider)",
        "sizeof(RigidbodyComponent)",
    };
    return index < kEntryCount ? kNames[index] : "unknown entry";
}

struct Mismatch {
    std::uint32_t index;
    std::uint32_t host;
    std::uint32_t dll;
};

// Compares the entries both sides know. A newer or older side may have more
// entries; the extra ones are not compared (they did not exist on the other side).
inline std::vector<Mismatch> Compare(const std::uint32_t* host, std::size_t hostCount,
                                     const std::uint32_t* dll, std::size_t dllCount) {
    std::vector<Mismatch> out;
    if (!host || !dll)
        return out;
    const std::size_t n = hostCount < dllCount ? hostCount : dllCount;
    for (std::size_t i = 0; i < n; ++i) {
        if (host[i] != dll[i])
            out.push_back({static_cast<std::uint32_t>(i), host[i], dll[i]});
    }
    return out;
}

// True when the only differences are the build flavour (Debug vs Release),
// which also changes every STL-holding size.
inline bool IsBuildFlavourMismatch(const std::vector<Mismatch>& mismatches) {
    for (const Mismatch& m : mismatches) {
        if (m.index == kIteratorDebugLevel || m.index == kDebugCrt)
            return true;
    }
    return false;
}

// Multi-line log text naming the DLL, the reason and the fix.
inline std::string Describe(const std::vector<Mismatch>& mismatches, const std::string& dllPath) {
    if (mismatches.empty())
        return {};
    std::string text = "[ScriptFactory] GameScripts layout does not match this engine build; "
                       "not loading " + dllPath + "\n";
    if (IsBuildFlavourMismatch(mismatches)) {
        text += "  Cause: Debug / Release mismatch. Build GameScripts in the same configuration "
                "as this editor / runtime (Debug with Debug, Release with Release).\n";
    } else {
        text += "  Cause: engine headers changed since GameScripts was built. "
                "Rebuild GameScripts against this engine.\n";
    }
    for (const Mismatch& m : mismatches) {
        text += "  " + std::string(EntryName(m.index)) + ": engine " + std::to_string(m.host) +
                ", GameScripts " + std::to_string(m.dll) + "\n";
    }
    return text;
}

}  // namespace script_abi
