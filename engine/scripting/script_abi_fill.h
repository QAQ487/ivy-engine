// script_abi_fill.h
// Fills the script_abi entry list with the values of the binary that compiles
// this header. GameScripts.dll compiles it through script_dll_exports.cpp
// (export IvyScriptAbiLayout); the host compiles it in script_factory.cpp.

#pragma once

#include "engine/scripting/script_abi_layout.h"
#include "engine/scripting/Script.h"
#include "engine/ecs/component.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/rigidbody_component.h"
#include "engine/ecs/transform.h"
#include "engine/physics/collider/collider.h"
#include "engine/physics/core/rigidbody.h"
#include "libraries/nlohmann/json.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace script_abi {

// Writes min(capacity, kEntryCount) entries to out and returns kEntryCount.
inline std::uint32_t FillLayout(std::uint32_t* out, std::uint32_t capacity) {
#if defined(_ITERATOR_DEBUG_LEVEL)
    constexpr std::uint32_t kIdl = _ITERATOR_DEBUG_LEVEL;
#else
    constexpr std::uint32_t kIdl = 0;
#endif
#if defined(_DEBUG)
    constexpr std::uint32_t kDebug = 1;
#else
    constexpr std::uint32_t kDebug = 0;
#endif
    const std::uint32_t values[] = {
        kAbiRevision,
        kIdl,
        kDebug,
        static_cast<std::uint32_t>(sizeof(void*)),
        static_cast<std::uint32_t>(sizeof(std::string)),
        static_cast<std::uint32_t>(sizeof(std::vector<int>)),
        static_cast<std::uint32_t>(sizeof(std::unordered_map<int, int>)),
        static_cast<std::uint32_t>(sizeof(std::function<void()>)),
        static_cast<std::uint32_t>(sizeof(std::shared_ptr<int>)),
        static_cast<std::uint32_t>(sizeof(nlohmann::json)),
        static_cast<std::uint32_t>(sizeof(GameObject)),
        static_cast<std::uint32_t>(sizeof(Component)),
        static_cast<std::uint32_t>(sizeof(Script)),
        static_cast<std::uint32_t>(sizeof(Rigidbody)),
        static_cast<std::uint32_t>(sizeof(engine::Transform)),
        static_cast<std::uint32_t>(sizeof(Collider)),
        static_cast<std::uint32_t>(sizeof(RigidbodyComponent)),
    };
    static_assert(sizeof(values) / sizeof(values[0]) == kEntryCount,
                  "script_abi: one value per Entry, in Entry order");
    if (out) {
        const std::uint32_t n = capacity < kEntryCount ? capacity : kEntryCount;
        for (std::uint32_t i = 0; i < n; ++i)
            out[i] = values[i];
    }
    return kEntryCount;
}

}  // namespace script_abi
