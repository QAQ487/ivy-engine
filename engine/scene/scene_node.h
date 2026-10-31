/*****************************************************
 * File: engine/scene_node.h
 * Project: d3d11_engine
 * -----------------------------------------------------------------------------
 * REFACTOR (2026-03-13)
 *   engine::GameObject is now a thin alias for ::GameObject.
 *   All prefab / script / serializer code that includes this header and uses
 *   engine::GameObject continues to compile without any changes.
 *
 *   The full class definition lives in game_object.h.
 *****************************************************/
#pragma once

#include "engine/ecs/game_object.h"   // ::GameObject – the one true scene-graph node

namespace engine {
    // Alias so existing code using engine::GameObject keeps compiling.
    using GameObject = ::GameObject;
} // namespace engine
