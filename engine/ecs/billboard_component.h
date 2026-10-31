/*****************************************************
 * File: engine/ecs/billboard_component.h
 * Project: d3d11_engine
 * Author: Terry Yeung
 * Created: 2026-06-07
 * Description: BillboardComponent  - declarative textured quad visuals
 *              separate from FBX-driven VisualComponent.
 *
 * Registered with ComponentFactory as "Billboard".
 *
 * Prefab JSON example:
 *   "Billboard": {
 *     "texture": "rom/texture/point_area_circle.png",
 *     "sizeX": 5.0,
 *     "sizeY": 5.0,
 *     "color": { "r": 1.0, "g": 0.95, "b": 0.55, "a": 0.88 },
 *     "rotation": 0.0,
 *     "groundAligned": true,
 *     "enabled": true
 *   }
 *****************************************************/
#pragma once
#include "component.h"
#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>

struct ID3D11ShaderResourceView;
class GameObject;

class BillboardComponent : public Component {
public:
    std::string texturePath;
    float sizeX = 1.0f;
    float sizeY = 1.0f;
    DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    float rotation = 0.0f;
    bool groundAligned = false;
    bool enabled = true;

    const char* GetTypeName() const override { return "Billboard"; }

    std::string Serialize() const override;
    void Deserialize(const std::string& jsonStr) override;

    ID3D11ShaderResourceView* GetTextureSRV();

    void InvalidateTextureCache() {
        m_textureId = -1;
        m_textureSrv = nullptr;
    }

    static void SubmitSceneBillboards(
        const std::vector<std::unique_ptr<GameObject>>& roots);

private:
    int m_textureId = -1;
    ID3D11ShaderResourceView* m_textureSrv = nullptr;

    static void SubmitNodeBillboards(GameObject* node);
};
