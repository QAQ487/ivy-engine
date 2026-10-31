// physics2d.cpp
// Box2D integration entry points.

#include "physics2d.h"
#include "physics2d_world.h"
#include "engine/ecs/physics2d_components.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/prefab_runtime.h"
#include "engine/scene/scene_loader.h"
#include "engine/scripting/script_component.h"
#include "engine/scripting/Script.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <set>
#include <unordered_map>

namespace {
Physics2DWorld world;
struct Entry {GameObject* object=nullptr;std::uint64_t key=0;DirectX::XMFLOAT3 scale{};};
std::unordered_map<GameObject*,Entry> entries;
std::unordered_map<std::uint64_t,GameObject*> identities;
std::uint64_t nextKey=1;
std::unordered_map<GameObject*,DirectX::XMFLOAT3> initialScales;
double accumulator=0;
constexpr float step=1.f/60.f;
GameObject* Live(std::uint64_t key) {const auto it=identities.find(key);return it==identities.end()?nullptr:it->second;}
bool Pose(GameObject* node,DirectX::XMFLOAT3& p,DirectX::XMFLOAT3& r,DirectX::XMFLOAT3& scale) {
    auto* t=node->GetTransform();if(!t)return false;t->GetWorldPositionRotationScale(p,r,scale);
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(r.z)&&std::isfinite(scale.x)&&std::isfinite(scale.y)
        &&std::fabs(scale.x)>1e-5f&&std::fabs(scale.y)>1e-5f&&std::fabs(r.x)<.001f&&std::fabs(r.y)<.001f;
}
bool IsPhysicsRoot(GameObject* node) {
    auto* parent=node->GetParent(); if(!parent)return true;
    if(!PrefabRuntime_IsSpawnRoot(parent))return false;
    DirectX::XMFLOAT3 p,r,s;
    if(!Pose(parent,p,r,s))return false;
    return std::fabs(p.x)<1e-5f&&std::fabs(p.y)<1e-5f&&std::fabs(r.z)<1e-5f
        &&std::fabs(s.x-1)<1e-5f&&std::fabs(s.y-1)<1e-5f;
}
void Sync() {
    std::set<GameObject*> seen;
    std::function<void(GameObject*)> visit=[&](GameObject* node) {
        if(!node||!node->IsActive())return;
        auto* collider=node->GetComponent<Collider2DComponent>();
        auto* body=node->GetComponent<Rigidbody2DComponent>();
        DirectX::XMFLOAT3 p,r,scale;
        bool valid=collider&&collider->enabled&&Pose(node,p,r,scale);
        if(valid) valid=std::isfinite(collider->size.x)&&collider->size.x>0&&std::isfinite(collider->size.y)&&collider->size.y>0
            &&std::isfinite(collider->radius)&&collider->radius>0&&std::isfinite(collider->offset.x)&&std::isfinite(collider->offset.y)
            &&std::isfinite(collider->friction)&&collider->friction>=0&&std::isfinite(collider->restitution)&&collider->restitution>=0&&collider->restitution<=1;
        if(valid&&body) valid=std::isfinite(body->mass)&&body->mass>0&&std::isfinite(body->gravityScale)
            &&std::isfinite(body->linearDamping)&&body->linearDamping>=0&&std::isfinite(body->velocity.x)&&std::isfinite(body->velocity.y);
        const bool moving=body&&body->bodyType!=BodyType2D::Static;
        if(valid && moving) valid=IsPhysicsRoot(node)&&std::fabs(std::fabs(scale.x)-std::fabs(scale.y))<.001f;
        if(valid && (moving||collider->shape==Shape2D::Circle)) valid=std::fabs(std::fabs(scale.x)-std::fabs(scale.y))<.001f;
        if(valid && initialScales.count(node)) {
            const auto& initial=initialScales.at(node);
            valid=std::fabs(initial.x-scale.x)<.001f&&std::fabs(initial.y-scale.y)<.001f;
        }
        if(valid) {
            seen.insert(node);
            initialScales.emplace(node,scale);
            if(!entries.count(node)) {
                const auto key=nextKey++;PhysicsBody2DSpec spec;
                spec.x=p.x;spec.y=p.y;spec.angle=DirectX::XMConvertToRadians(r.z);
                spec.type=!body||body->bodyType==BodyType2D::Static?0:body->bodyType==BodyType2D::Dynamic?1:2;
                spec.circle=collider->shape==Shape2D::Circle;spec.sensor=collider->isTrigger;
                spec.halfWidth=(std::max)(.0001f,collider->size.x*std::fabs(scale.x)*.5f);
                spec.halfHeight=(std::max)(.0001f,collider->size.y*std::fabs(scale.y)*.5f);
                spec.radius=(std::max)(.0001f,collider->radius*(std::max)(std::fabs(scale.x),std::fabs(scale.y)));
                spec.offsetX=collider->offset.x*scale.x;spec.offsetY=collider->offset.y*scale.y;
                spec.friction=collider->friction;spec.restitution=collider->restitution;
                spec.category=collider->categoryBits;spec.mask=collider->maskBits;
                if(body) {spec.mass=body->mass;spec.gravityScale=body->gravityScale;spec.damping=body->linearDamping;spec.fixedRotation=body->fixedRotation;spec.bullet=body->bullet;}
                if(!world.Add(key,spec)) {for(const auto& child:node->GetChildren())visit(child.get());return;}
                entries[node]={node,key,scale};identities[key]=node;
                if(body)world.SetVelocity(key,body->velocity.x,body->velocity.y);
            }
            if(!moving) world.SetTransform(entries.at(node).key,p.x,p.y,DirectX::XMConvertToRadians(r.z));
        }
        for(const auto& child:node->GetChildren())visit(child.get());
    };
    for(const auto& root:SceneLoader_GetSceneNodes())visit(root.get());
    std::vector<GameObject*> remove;
    for(const auto& entry:entries) if(!seen.count(entry.first))remove.push_back(entry.first);
    for(auto* node:remove) {auto baseline=initialScales.find(node); auto saved=baseline==initialScales.end()?DirectX::XMFLOAT3{}:baseline->second; const bool had=baseline!=initialScales.end(); Physics2D_Remove(node); if(had)initialScales[node]=saved;}
}
}
void Physics2D_Reset() {world.Clear();entries.clear();identities.clear();initialScales.clear();accumulator=0;}
void Physics2D_Remove(GameObject* node) {
    initialScales.erase(node);
    const auto it=entries.find(node);if(it==entries.end())return;
    world.Remove(it->second.key);identities.erase(it->second.key);entries.erase(it);
}
void Physics2D_Update(double dt) {
    Sync();
    if(!std::isfinite(dt)||dt<=0)return;
    accumulator+=(std::min)(dt,.25);
    int count=0;
    while(accumulator+1e-9>=1.0/60.0 && count++<16) {
        for(const auto& entry:entries) {
            auto* body=entry.first->GetComponent<Rigidbody2DComponent>();if(!body||body->bodyType==BodyType2D::Static)continue;
            auto velocity=body->velocity;
            if(body->bodyType==BodyType2D::Kinematic&&body->hasMoveTarget) {
                PhysicsBody2DState state;world.State(entry.second.key,state);
                velocity={(body->moveTarget.x-state.x)/step,(body->moveTarget.y-state.y)/step};
            }
            if(std::isfinite(velocity.x)&&std::isfinite(velocity.y))world.SetVelocity(entry.second.key,velocity.x,velocity.y);
        }
        world.Step(step);
        for(const auto& entry:entries) {
            auto* body=entry.first->GetComponent<Rigidbody2DComponent>();if(!body||body->bodyType==BodyType2D::Static)continue;
            PhysicsBody2DState state;if(!world.State(entry.second.key,state))continue;
            auto* transform=entry.first->GetTransform();
            transform->localPosition.x=state.x;transform->localPosition.y=state.y;transform->localRotation.z=DirectX::XMConvertToDegrees(state.angle);
            body->velocity=body->hasMoveTarget?DirectX::XMFLOAT2{}:DirectX::XMFLOAT2{state.vx,state.vy};body->hasMoveTarget=false;
            entry.first->SyncLegacyTransformFromComponents();
        }
        accumulator-=1.0/60.0;
    }
}
void Physics2D_DispatchEvents() {
    auto events=world.TakeEvents();
    for(const auto& event:events) for(int side=0;side<2;++side) {
        auto* self=Live(side?event.b:event.a);auto* other=Live(side?event.a:event.b);
        if(!self || (event.enter&&!other))continue;
        auto* script=self->GetComponent<ScriptComponent>();
        auto* listener=script?dynamic_cast<Physics2DListener*>(script->GetScript()):nullptr;
        if(listener) {const float sign=side?-1.f:1.f;Physics2DEvent data{other,event.enter,event.trigger,{event.px,event.py},{event.nx*sign,event.ny*sign}};listener->OnPhysics2DEvent(*self,data);}
    }
}
bool Physics2D_Raycast(DirectX::XMFLOAT2 origin,DirectX::XMFLOAT2 direction,float distance,Physics2DRayHit& hit,std::uint32_t mask,bool sensors) {
    hit={};PhysicsRay2D result;
    if(!world.Raycast(origin.x,origin.y,direction.x,direction.y,distance,mask,sensors,result))return false;
    hit.object=Live(result.key);hit.point={result.x,result.y};hit.normal={result.nx,result.ny};hit.distance=result.distance;
    return hit.object!=nullptr;
}


#include "engine/core/debug_system.h"
#include "engine/debug_draw/debug_drawer.h"
void Physics2D_DrawDebugColliders() {
    using namespace DirectX;
    std::vector<XMFLOAT3> solids,triggers;
    const bool all=DebugSystem::CollisionVizEnabled();
    std::function<void(GameObject*)> visit=[&](GameObject* node) {
        if(!node||!node->IsActive())return;
        auto* collider=node->GetComponent<Collider2DComponent>();
        XMFLOAT3 p,r,scale;
        if(collider&&collider->enabled&&(all||collider->showDebugCollider)&&Pose(node,p,r,scale)) {
            const float angle=XMConvertToRadians(r.z),c=std::cos(angle),s=std::sin(angle);
            auto point=[&](float x,float y) {
                x=(x+collider->offset.x)*scale.x;y=(y+collider->offset.y)*scale.y;
                return XMFLOAT3{p.x+c*x-s*y,p.y+s*x+c*y,p.z};
            };
            std::vector<XMFLOAT3> lines;
            if(collider->shape==Shape2D::Circle) {
                if(std::isfinite(collider->radius)&&collider->radius>0&&std::fabs(std::fabs(scale.x)-std::fabs(scale.y))<.001f) {
                    constexpr int segments=64;
                    for(int i=0;i<segments;++i) {
                        const float a=XM_2PI*i/segments,b=XM_2PI*(i+1)/segments;
                        lines.push_back(point(std::cos(a)*collider->radius,std::sin(a)*collider->radius));
                        lines.push_back(point(std::cos(b)*collider->radius,std::sin(b)*collider->radius));
                    }
                }
            } else if(std::isfinite(collider->size.x)&&std::isfinite(collider->size.y)&&collider->size.x>0&&collider->size.y>0) {
                const float x=collider->size.x*.5f,y=collider->size.y*.5f;
                const XMFLOAT3 corners[]={point(-x,-y),point(x,-y),point(x,y),point(-x,y)};
                for(int i=0;i<4;++i) {lines.push_back(corners[i]);lines.push_back(corners[(i+1)%4]);}
            }
            bool finite=true;
            for(const auto& v:lines)finite=finite&&std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);
            if(finite) {auto& target=collider->isTrigger?triggers:solids;target.insert(target.end(),lines.begin(),lines.end());}
        }
        for(const auto& child:node->GetChildren())visit(child.get());
    };
    for(const auto& root:SceneLoader_GetSceneNodes())visit(root.get());
    if(solids.empty()&&triggers.empty())return;
    static DebugDrawer drawer;
    if(!drawer.Initialize())return;
    Shader3D_Begin();SetDepthEnable(false);
    const auto& camera=GetCamera();
    Shader_SetProjectionMatrix(camera.GetProjMatrix(GetRenderViewportWidth(),GetRenderViewportHeight()));
    Shader_SetViewMatrix(camera.GetViewMatrix());
    drawer.DrawLinesCached(solids,{0,1,0,1});
    drawer.DrawLinesCached(triggers,{1,1,0,1});
    SetDepthEnable(true);
}
