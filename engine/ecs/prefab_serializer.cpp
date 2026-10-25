/*****************************************************
 * File: prefab_serializer.cpp
 * Project: d3d11_engine
 * Author: Terry Yeung
 * Created: 2026-03-12
 * -----------------------------------------------------------------------------
 * REFACTOR (2026-03-13)
 *   engine::GameObject ¨ ::GameObject throughout (alias, no logic change).
 *****************************************************/
#include "prefab_serializer.h"
#include "component_factory.h"
#include "tag_registry.h"
#include "engine/scene/scene_json_validation.h"

#include <fstream>
#include <sstream>
#include <Windows.h>

#include "libraries/nlohmann/json.hpp"

using json = nlohmann::json;

// „Ÿ„Ÿ Internal helpers „Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ

static json SerializeNode(const GameObject& node) {
    json j;
    j["name"] = node.GetName();
    j["tag"] = node.GetTagId();
    if (!node.GetPrefabAssetPath().empty())
        j["prefabAssetPath"] = node.GetPrefabAssetPath();

    // Serialize every component.
    json comps = json::object();
    for (const auto& kv : node.GetComponents()) {
        const Component* comp = kv.second.get();
        std::string compJson  = comp->Serialize();
        try {
            comps[comp->GetTypeName()] = json::parse(compJson);
        } catch (...) {
            comps[comp->GetTypeName()] = compJson;
        }
    }
    j["components"] = comps;

    // Recurse into children.
    json children = json::array();
    for (const auto& child : node.GetChildren())
        if (child) children.push_back(SerializeNode(*child));
    j["children"] = children;

    return j;
}

static std::unique_ptr<GameObject> DeserializeNode(const json& j, bool strict = false, bool children = true) {
    if (strict) {
        if (!j.is_object()) throw std::runtime_error("scene node must be an object");
        if (j.contains("components") && !j["components"].is_object()) throw std::runtime_error("components must be an object");
        if (j.contains("children") && !j["children"].is_array()) throw std::runtime_error("children must be an array");
    }
    std::string name = j.value("name", "GameObject");
    auto node = std::make_unique<GameObject>(name);

    const int tagValue = j.value("tag", static_cast<int>(Tag::TAG_NONE));
    if (TagRegistry::Instance().IsValid(tagValue))
        node->SetTag(tagValue);

    if (j.contains("prefabAssetPath"))
        node->SetPrefabAssetPath(j["prefabAssetPath"].get<std::string>());

    // Reconstruct components via ComponentFactory.
    if (j.contains("components") && j["components"].is_object()) {
        for (auto it = j["components"].begin(); it != j["components"].end(); ++it) {
            const std::string& typeName = it.key();
            auto comp = ComponentFactory::Instance().Create(typeName);
            if (!comp) {
                OutputDebugStringA(("[PrefabSerializer] Unknown component: "
                                    + typeName + "\n").c_str());
                continue;
            }
            std::string data = it.value().is_string()
                               ? it.value().get<std::string>()
                               : it.value().dump();
            if (strict) SceneJson_ValidateTypes(json::parse(data), json::parse(comp->Serialize()), typeName);
            comp->Deserialize(data);
            if (strict) node->AddPreparedComponent(std::move(comp));
            else node->AddComponentRaw(std::move(comp));
        }
    }

    // Recurse into children.
    if (children && j.contains("children") && j["children"].is_array()) {
        for (const auto& childJson : j["children"]) {
            auto child = DeserializeNode(childJson, strict);
            if (child) node->AddChild(std::move(child));
        }
    }

    return node;
}

static const json* FindPrefabNodeJson(const json& root) {
    if (!root.is_object()) return nullptr;

    if (root.contains("nodes") && root["nodes"].is_array()) {
        for (const auto& nodeJson : root["nodes"]) {
            if (nodeJson.is_object())
                return &nodeJson;
        }
        return nullptr;
    }

    if (root.contains("prefab") && root["prefab"].is_object())
        return &root["prefab"];

    return &root;
}

// „Ÿ„Ÿ Public API „Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ

std::string PrefabSerializer::Serialize(const GameObject& root) {
    return SerializeNode(root).dump(4);
}

bool PrefabSerializer::SaveToFile(const GameObject& root,
                                   const std::string& filePath) {
    std::ofstream f(filePath);
    if (!f.is_open()) {
        OutputDebugStringA(("[PrefabSerializer] Cannot open for write: "
                            + filePath + "\n").c_str());
        return false;
    }

    json prefab;
    prefab["version"] = 4;
    prefab["nodes"] = json::array({ SerializeNode(root) });
    f << prefab.dump(4);
    return true;
}

std::unique_ptr<GameObject> PrefabSerializer::Deserialize(
    const std::string& jsonStr)
{
    try {
        json root = json::parse(jsonStr);
        const json* nodeJson = FindPrefabNodeJson(root);
        if (!nodeJson) {
            OutputDebugStringA("[PrefabSerializer] No prefab node found.\n");
            return nullptr;
        }
        return DeserializeNode(*nodeJson);
    } catch (const nlohmann::json::parse_error& e) {
        OutputDebugStringA(("[PrefabSerializer] Parse error: "
                            + std::string(e.what()) + "\n").c_str());
        return nullptr;
    } catch (...) {
        OutputDebugStringA("[PrefabSerializer] Unknown error.\n");
        return nullptr;
    }
}

std::unique_ptr<GameObject> PrefabSerializer::LoadFromFile(
    const std::string& filePath)
{
    std::ifstream f(filePath);
    if (!f.is_open()) {
        OutputDebugStringA(("[PrefabSerializer] Cannot open: " + filePath + "\n").c_str());
        return nullptr;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return Deserialize(ss.str());
}

std::unique_ptr<GameObject> PrefabSerializer::DeserializeForScene(const std::string& data) {
    const auto root = json::parse(data);
    const auto* node = FindPrefabNodeJson(root);
    if (!node) throw std::runtime_error("No prefab node found");
    return DeserializeNode(*node, true);
}

std::unique_ptr<GameObject> PrefabSerializer::DeserializeSceneNode(const json& node) { return DeserializeNode(node, true, false); }
