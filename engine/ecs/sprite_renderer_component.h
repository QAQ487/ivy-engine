#pragma once
#include "component.h"
#include <DirectXMath.h>

class SpriteRendererComponent : public Component {
public:
    std::string texturePath;
    float pixelsPerUnit = 100.0f;
    DirectX::XMFLOAT4 sourceRect = {0,0,0,0}; // x,y,width,height; zero size = full texture
    DirectX::XMFLOAT2 pivot = {0.5f,0.5f}; // measured from bottom-left
    DirectX::XMFLOAT4 tint = {1,1,1,1};
    // Transient world-space presentation offset. Kept out of serialization so
    // scripts can add visual-only motion without moving colliders or gameplay.
    DirectX::XMFLOAT2 renderOffset = {0,0};
    // Transient RGB override for short presentation flashes. Alpha remains the
    // authored tint alpha so hidden sprites stay hidden during an effect.
    DirectX::XMFLOAT4 renderTint = {1,1,1,1};
    bool hasRenderTint = false;
    bool flipX = false, flipY = false;
    int sortingLayer = 0, orderInLayer = 0;
    const char* GetTypeName() const override { return "SpriteRenderer"; }
    std::string Serialize() const override;
    void Deserialize(const std::string&) override;
    void Draw() const;
    std::string EffectiveTexture() const;
    DirectX::XMFLOAT4 EffectiveRect() const;
    void SnapshotStartState() {m_startJson=Serialize();}
    void OnReset() override {if(!m_startJson.empty()) Deserialize(m_startJson); renderOffset={0,0}; renderTint={1,1,1,1}; hasRenderTint=false;}
    void OnAttach() override {SnapshotStartState();}
    DirectX::XMFLOAT2 GetWorldSize() const;
    bool HitTestRay(DirectX::FXMVECTOR origin, DirectX::FXMVECTOR direction) const;
private:
    std::string m_startJson;
    mutable std::string loadedPath;
    mutable int loadedTexture = -1;
};

// Called on the rendering thread alongside scene model rendering.
void DrawSceneSprites();
