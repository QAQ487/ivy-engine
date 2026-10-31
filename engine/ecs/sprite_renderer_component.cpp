// sprite_renderer_component.cpp
// ECS: sprite renderer component implementation.

#include "sprite_animator_component.h"
#include "engine/rendering/sprite/sprite_pick_math.h"
#include <cmath>
#include "sprite_renderer_component.h"
#include "engine/scene/scene_loader.h"
#include "engine/rendering/sprite/sprite.h"
#include "engine/rendering/sprite/texture.h"
#include "engine/rendering/d3d11/renderer.h"
#include "engine/core/utf8_path.h"
#include "camera.h"
#include <algorithm>
#include <filesystem>
#include <functional>
#include <tuple>

using json = nlohmann::json;
std::string SpriteRendererComponent::Serialize() const {
    return json{{"texture",texturePath},{"pixelsPerUnit",pixelsPerUnit},
        {"sourceRect",{sourceRect.x,sourceRect.y,sourceRect.z,sourceRect.w}},
        {"pivot",{pivot.x,pivot.y}},{"tint",{tint.x,tint.y,tint.z,tint.w}},
        {"flipX",flipX},{"flipY",flipY},{"sortingLayer",sortingLayer},{"orderInLayer",orderInLayer}}.dump();
}
void SpriteRendererComponent::Deserialize(const std::string& data) {
    const auto j = json::parse(data);
    texturePath = j.value("texture",std::string{});
    pixelsPerUnit = j.value("pixelsPerUnit",100.0f);
    if (!(pixelsPerUnit > 0.001f)) pixelsPerUnit = 100.0f;
    auto r = j.value("sourceRect",std::vector<float>{0,0,0,0});
    if(r.size()==4) sourceRect={r[0],r[1],r[2],r[3]};
    auto p = j.value("pivot",std::vector<float>{0.5f,0.5f});
    if(p.size()==2) pivot={p[0],p[1]};
    auto c = j.value("tint",std::vector<float>{1,1,1,1});
    if(c.size()==4) tint={c[0],c[1],c[2],c[3]};
    flipX=j.value("flipX",false); flipY=j.value("flipY",false);
    sortingLayer=j.value("sortingLayer",0); orderInLayer=j.value("orderInLayer",0);
}
void SpriteRendererComponent::Draw() const {
    if (tint.w == 0.0f) return;
    const auto texturePath=EffectiveTexture();
    const auto sourceRect=EffectiveRect();
    if (!owner || texturePath.empty() || !(pixelsPerUnit > 0.001f)) return;
    if (loadedPath != texturePath) {
        loadedPath = texturePath;
        const auto path = PathFromUtf8(texturePath).wstring();
        loadedTexture = TextureLoadQuiet(path.c_str(), true);
    }
    const int texture = loadedTexture;
    if(texture < 0) return;
    const auto size = TextureGetSize(texture);
    if(!size.x || !size.y) return;
    const float w=sourceRect.z>0?sourceRect.z:float(size.x);
    const float h=sourceRect.w>0?sourceRect.w:float(size.y);
    DirectX::XMFLOAT4 uv{sourceRect.x/size.x,sourceRect.y/size.y,
        (sourceRect.x+w)/size.x,(sourceRect.y+h)/size.y};
    auto drawTint = tint;
    if (hasRenderTint) {
        drawTint.x = renderTint.x;
        drawTint.y = renderTint.y;
        drawTint.z = renderTint.z;
    }
    if(flipX) std::swap(uv.x,uv.z);
    if(flipY) std::swap(uv.y,uv.w);
    const auto* transform=owner->GetComponent<engine::Transform>();
    const auto& camera=GetCamera();
    auto world = transform ? transform->GetWorldMatrix() : DirectX::XMMatrixIdentity();
    world.r[3] = DirectX::XMVectorAdd(world.r[3], DirectX::XMVectorSet(renderOffset.x, renderOffset.y, 0.0f, 0.0f));
    Sprite_DrawWorld(texture,w/pixelsPerUnit,h/pixelsPerUnit,pivot,uv,drawTint,
        world,
        camera.GetViewMatrix(),camera.GetProjMatrix(GetRenderViewportWidth(),GetRenderViewportHeight()));
}
void DrawSceneSprites() {
    std::vector<SpriteRendererComponent*> sprites;
    std::function<void(GameObject*)> visit = [&](GameObject* node) {
        if(!node || !node->IsActive()) return;
        if(auto* sprite=node->GetComponent<SpriteRendererComponent>(); sprite && sprite->tint.w != 0.0f)
            sprites.push_back(sprite);
        for(const auto& child:node->GetChildren()) visit(child.get());
    };
    for(const auto& root:SceneLoader_GetSceneNodes()) visit(root.get());
    std::stable_sort(sprites.begin(),sprites.end(),[](const auto* a,const auto* b){
        return std::tie(a->sortingLayer,a->orderInLayer)<std::tie(b->sortingLayer,b->orderInLayer);
    });
    for(const auto* sprite:sprites) sprite->Draw();
}

DirectX::XMFLOAT2 SpriteRendererComponent::GetWorldSize() const {
    const auto texturePath=EffectiveTexture();
    const auto sourceRect=EffectiveRect();
    if(texturePath.empty() || !(pixelsPerUnit>0.001f)) return {};
    if(loadedPath!=texturePath) {
        loadedPath=texturePath;
        loadedTexture=TextureLoadQuiet(PathFromUtf8(texturePath).wstring().c_str());
    }
    if(loadedTexture<0) return {};
    const auto size=TextureGetSize(loadedTexture);
    return {(sourceRect.z>0?sourceRect.z:float(size.x))/pixelsPerUnit,
            (sourceRect.w>0?sourceRect.w:float(size.y))/pixelsPerUnit};
}
bool SpriteRendererComponent::HitTestRay(DirectX::FXMVECTOR origin, DirectX::FXMVECTOR direction) const {
    using namespace DirectX;
    if(!owner) return false;
    const auto size=GetWorldSize();
    if(size.x<=0 || size.y<=0) return false;
    const auto* transform=owner->GetComponent<engine::Transform>();
    if(!transform) return false;
    return SpriteQuadHit(origin,direction,transform->GetWorldMatrix(),size,pivot);
}

std::string SpriteRendererComponent::EffectiveTexture() const {
    if(owner) if(const auto* animator=owner->GetComponent<SpriteAnimatorComponent>())
        if(const auto* clip=animator->CurrentClip(); clip && !clip->texture.empty()) return clip->texture;
    return texturePath;
}
DirectX::XMFLOAT4 SpriteRendererComponent::EffectiveRect() const {
    if(owner) if(const auto* animator=owner->GetComponent<SpriteAnimatorComponent>())
        if(const auto* frame=animator->CurrentFrame()) return {frame->x,frame->y,frame->w,frame->h};
    return sourceRect;
}
