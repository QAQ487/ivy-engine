#pragma once
#include "box2d/box2d.h"
#include <cstdint>
#include <map>
#include <vector>

struct PhysicsBody2DSpec {
    int type=0; // static, dynamic, kinematic
    bool circle=false,sensor=false,fixedRotation=true,bullet=true;
    float x=0,y=0,angle=0,halfWidth=.5f,halfHeight=.5f,radius=.5f,offsetX=0,offsetY=0;
    float mass=1,gravityScale=0,damping=0,friction=.3f,restitution=0;
    std::uint32_t category=1,mask=0xffffffff;
};
struct PhysicsBody2DState {float x=0,y=0,angle=0,vx=0,vy=0;};
struct PhysicsContact2D {
    std::uint64_t a=0,b=0;
    bool enter=true,trigger=false;
    float px=0,py=0,nx=0,ny=0;
};
struct PhysicsRay2D {std::uint64_t key=0;float x=0,y=0,nx=0,ny=0,distance=0;};

// Owns only physics data. GameObject lifetime and script dispatch belong to the adapter.
class Physics2DWorld {
public:
    ~Physics2DWorld() {Clear();}
    bool Add(std::uint64_t key,const PhysicsBody2DSpec& spec);
    void Remove(std::uint64_t key);
    void Clear();
    void Step(float dt);
    void SetVelocity(std::uint64_t key,float x,float y);
    void SetTransform(std::uint64_t key,float x,float y,float angle);
    bool State(std::uint64_t key,PhysicsBody2DState& state) const;
    bool Raycast(float x,float y,float dx,float dy,float distance,std::uint32_t mask,bool sensors,PhysicsRay2D& hit) const;
    std::vector<PhysicsContact2D> TakeEvents();
private:
    struct Body {b2BodyId body; b2ShapeId shape;};
    b2WorldId m_world{};
    std::map<std::uint64_t,Body> m_bodies;
    std::map<std::pair<std::uint64_t,std::uint64_t>,PhysicsContact2D> m_pairs;
    std::vector<PhysicsContact2D> m_events;
    void Begin(b2ShapeId a,b2ShapeId b,bool trigger,const b2Manifold* manifold=nullptr);
    void End(b2ShapeId a,b2ShapeId b);
};
