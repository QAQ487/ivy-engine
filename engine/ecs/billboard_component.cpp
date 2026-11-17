/*****************************************************
 * File: engine/ecs/billboard_component.cpp
 * Project: d3d11_engine
 * Author: Terry Yeung
 * Created: 2026-06-07
 *****************************************************/
#include "billboard_component.h"
#include "transform.h"
#include "string_utils.h"
#include "engine/ecs/game_object.h"
#include "engine/rendering/billboard/billboard.h"
#include "engine/rendering/sprite/texture.h"
#include "libraries/nlohmann/json.hpp"
#include <Windows.h>
#include "engine/debug_draw/debug_ofstream.h"

ID3D11ShaderResourceView* BillboardComponent::GetTextureSRV() {
    if (m_textureId >= 0) return TextureGetSRV(m_textureId);
    if (texturePath.empty()) return nullptr;

    const std::wstring wpath = ToWString(texturePath);
    if (wpath.empty()) return nullptr;

    m_textureId = TextureLoad(wpath.c_str(), true);
    if (m_textureId < 0) return nullptr;

    m_textureSrv = TextureGetSRV(m_textureId);
    return m_textureSrv;
}

std::string BillboardComponent::Serialize() const {
    nlohmann::json j;
    j["texture"] = texturePath;
    j["sizeX"] = sizeX;
    j["sizeY"] = sizeY;
    j["color"] = {
        {"r", color.x},
        {"g", color.y},
        {"b", color.z},
        {"a", color.w}
    };
    j["rotation"] = rotation;
    j["groundAligned"] = groundAligned;
    j["enabled"] = enabled;
    return j.dump();
}

void BillboardComponent::Deserialize(const std::string& jsonStr) {
    try {
        auto j = nlohmann::json::parse(jsonStr);
        texturePath = j.value("texture", "");
        sizeX = j.value("sizeX", 1.0f);
        sizeY = j.value("sizeY", 1.0f);
        rotation = j.value("rotation", 0.0f);
        groundAligned = j.value("groundAligned", false);
        enabled = j.value("enabled", true);

        if (j.contains("color") && j["color"].is_object()) {
            const auto& c = j["color"];
            color.x = c.value("r", 1.0f);
            color.y = c.value("g", 1.0f);
            color.z = c.value("b", 1.0f);
            color.w = c.value("a", 1.0f);
        }
    } catch (...) {
        hal::DebugWriteA("[BillboardComponent] Deserialize: JSON parse error.\n");
    }
}

void BillboardComponent::SubmitNodeBillboards(GameObject* node) {
    if (!node || !node->IsActive()) return;

    if (auto* bc = node->GetComponent<BillboardComponent>()) {
        if (bc->enabled) {
            if (const engine::Transform* t = node->GetComponent<engine::Transform>()) {
                Billboard::Add(
                    t->GetWorldPosition(),
                    {bc->sizeX, bc->sizeY},
                    bc->color,
                    bc->GetTextureSRV(),
                    bc->rotation,
                    bc->groundAligned);
            }
        }
    }

    for (const auto& child : node->GetChildren())
        if (child) SubmitNodeBillboards(child.get());
}

void BillboardComponent::SubmitSceneBillboards(
    const std::vector<std::unique_ptr<GameObject>>& roots) {
    for (const auto& root : roots)
        if (root) SubmitNodeBillboards(root.get());
}
