#include "engine/ecs/sprite_renderer_component.h"
#include "engine/ecs/sprite_animator_component.h"
#include "engine/scene/scene_loader.h"
#include "engine/rendering/sprite/sprite.h"
#include "engine/rendering/sprite/texture.h"
#include "camera.h"
#include <cassert>
#include <cstdio>

// Isolate the real sprite renderer from audio, physics and the D3D device.
GameObject::~GameObject() = default;
void GameObject::OnTriggerEnter(GameObject*) {}
void GameObject::OnTriggerExit(GameObject*) {}
bool GameObject::IgnoresCollisionWith(const GameObject*) const { return false; }
DirectX::XMFLOAT3 Rigidbody_InvInertiaUnitCube(float) { return {}; }
void Rigidbody::Integrate(DirectX::XMFLOAT3&, DirectX::XMFLOAT3&, double) {}
DirectX::XMMATRIX engine::Transform::GetWorldMatrix() const { return DirectX::XMMatrixIdentity(); }
bool EngineGUI_IsPlaying() { return true; }
float GetRenderViewportWidth() { return 1280; }
float GetRenderViewportHeight() { return 720; }

static std::vector<std::unique_ptr<GameObject>> nodes;
std::vector<std::unique_ptr<GameObject>>& SceneLoader_GetSceneNodes() { return nodes; }
static int loads = 0;
static std::vector<int> draws;
int TextureLoadQuiet(const wchar_t*, bool) { return ++loads; }
DirectX::XMUINT2 TextureGetSize(int) { return {64,64}; }
void Sprite_DrawWorld(int texture, float, float, DirectX::XMFLOAT2,
    DirectX::XMFLOAT4, DirectX::XMFLOAT4, const DirectX::XMMATRIX&,
    const DirectX::XMMATRIX&, const DirectX::XMMATRIX&) {
    draws.push_back(texture);
}

int main() {
    auto root = std::make_unique<GameObject>();
    auto* hidden = root->AddComponent<SpriteRendererComponent>();
    hidden->texturePath = "hidden.png";
    hidden->tint.w = 0;
    auto child = std::make_unique<GameObject>();
    auto* visible = child->AddComponent<SpriteRendererComponent>();
    visible->texturePath = "visible.png";
    root->AddChild(std::move(child));
    nodes.push_back(std::move(root));

    hidden->Draw();
    assert(loads == 0 && draws.empty()); // Direct calls must also skip hidden sprites.
    DrawSceneSprites();
    assert(loads == 1 && draws == std::vector<int>{1}); // Hidden parents do not hide children.

    hidden->tint.w = 0.25f;
    hidden->orderInLayer = 1;
    draws.clear();
    DrawSceneSprites();
    assert(loads == 2 && draws == (std::vector<int>{1,2})); // Reappears in sorting order.

    hidden->tint.w = 0;
    draws.clear();
    DrawSceneSprites();
    assert(loads == 2 && draws == std::vector<int>{1});
    hidden->tint.w = 1;
    draws.clear();
    DrawSceneSprites();
    assert(loads == 2 && draws == (std::vector<int>{1,2})); // Reuses its loaded texture.

    nodes.front()->SetActive(false);
    draws.clear();
    DrawSceneSprites();
    assert(draws.empty());
    std::puts("PASS: sprite transparency, reappearance, ordering and child visibility");
}
