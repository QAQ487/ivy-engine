/*****************************************************
 * File: engine/scene_node.cpp
 * Project: d3d11_engine
 * -----------------------------------------------------------------------------
 * REFACTOR (2026-03-13)
 *   engine::GameObject is now an alias for ::GameObject (see scene_node.h).
 *   All method implementations that previously lived here have moved into
 *   game_object.h (inline) or game_object.cpp.
 *
 *   This file is intentionally empty – keep it in the project so existing
 *   build rules that reference scene_node.cpp continue to work.
 *****************************************************/
#include "scene_node.h"
