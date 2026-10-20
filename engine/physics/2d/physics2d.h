#pragma once
#include <DirectXMath.h>
#include <cstdint>
class GameObject;

struct Physics2DEvent {
    GameObject* other=nullptr; // null on exit if the other object was destroyed
    bool enter=true,trigger=false;
    DirectX::XMFLOAT2 point{},normal{}; // normal points from self toward the other collider
};
// Independent optional script interface: does not change Script's existing virtual ABI.
class Physics2DListener {
public:
    virtual ~Physics2DListener()=default;
    virtual void OnPhysics2DEvent(GameObject& self,const Physics2DEvent& event)=0;
};
struct Physics2DRayHit {GameObject* object=nullptr;DirectX::XMFLOAT2 point{},normal{};float distance=0;};
void Physics2D_Update(double dt);
void Physics2D_DispatchEvents();
void Physics2D_Remove(GameObject* object);
void Physics2D_Reset();
bool Physics2D_Raycast(DirectX::XMFLOAT2 origin,DirectX::XMFLOAT2 direction,float distance,
    Physics2DRayHit& hit,std::uint32_t mask=0xffffffff,bool includeTriggers=false);

// Draw scene collider outlines inside the bound viewport, before UI.
void Physics2D_DrawDebugColliders();
