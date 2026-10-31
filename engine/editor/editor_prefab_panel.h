/*****************************************************
 * File: engine/editor_prefab_panel.h
 * Project: d3d11_engine
 * Author: Terry Yeung
 * Created: 2026-03-12
 * -----------------------------------------------------------------------------
 * REFACTOR (2026-03-13)
 *   engine::GameObject  -> ::GameObject (alias, no API change).
 *****************************************************/
#pragma once

#include <memory>
#include <string>
#include <vector>

class GameObject;

namespace MapEditor {

GameObject* PrefabPanel(
    const std::string& prefabFolder,
    std::vector<std::unique_ptr<GameObject>>& nodes);

} // namespace MapEditor
