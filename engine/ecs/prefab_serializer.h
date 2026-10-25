/*****************************************************
 * File: prefab_serializer.h
 * Project: d3d11_engine
 * Author: Terry Yeung
 * Created: 2026-03-12
 * -----------------------------------------------------------------------------
 * REFACTOR (2026-03-13)
 *   Uses ::GameObject directly (engine::GameObject is now a thin alias).
 *   All method signatures are unchanged.
 *
 * Node JSON format (recursive):
 * {
 *   "name": "EnemyManager",
 *   "prefabAssetPath": "assets/Prefabs/EnemyManager.prefab.json",
 *   "components": {
 *     "SceneTransform": { ...component JSON... },
 *     "visual":         { "model": "...", "proxy_model": "..." },
 *     "EnemyManager":   { ...component JSON... }
 *   },
 *   "children": [ { ...same format... }, ... ]
 * }
 *
 * Prefab files saved by SaveToFile use the current scene-style wrapper:
 * { "version": 4, "nodes": [ { ...node JSON... } ] }
 * Load/Deserialize still accepts old bare-node prefab files.
 *****************************************************/
#pragma once
#include <memory>
#include <string>
#include "libraries/nlohmann/json.hpp"
#include "engine/ecs/game_object.h"

class PrefabSerializer {
public:
    static std::unique_ptr<GameObject> DeserializeSceneNode(const nlohmann::json& node);
    // „Ÿ„Ÿ Save „Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ
    static std::string Serialize(const GameObject& root);
    static bool        SaveToFile(const GameObject& root, const std::string& filePath);

    // „Ÿ„Ÿ Load „Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ
    // Returns nullptr on parse error.
    static std::unique_ptr<GameObject> DeserializeForScene(const std::string& json);
    static std::unique_ptr<GameObject> Deserialize(const std::string& json);
    static std::unique_ptr<GameObject> LoadFromFile(const std::string& filePath);
};
