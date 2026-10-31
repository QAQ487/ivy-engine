// physics2d_world.cpp
// 2D physics world wrapper around Box2D.

#include "physics2d_world.h"
#include <algorithm>
#include <cmath>
#include <utility>

static std::uint64_t ShapeKey(b2ShapeId shape) {
    return b2Shape_IsValid(shape)?std::uint64_t(reinterpret_cast<std::uintptr_t>(b2Shape_GetUserData(shape))):0;
}
bool Physics2DWorld::Add(std::uint64_t key,const PhysicsBody2DSpec& s) {
    auto bounded=[](float v) {return std::isfinite(v)&&std::fabs(v)<=100000.f;};
    for(float v:{s.x,s.y,s.angle,s.halfWidth,s.halfHeight,s.radius,s.offsetX,s.offsetY,s.mass,s.gravityScale,s.damping,s.friction,s.restitution})
        if(!bounded(v))return false;
    if(!key||s.halfWidth<=0||s.halfHeight<=0||s.radius<=0||s.mass<=0||s.damping<0||s.friction<0||s.restitution<0||s.restitution>1)return false;
    const double area=s.circle?3.141592653589793*double(s.radius)*s.radius:4.0*double(s.halfWidth)*s.halfHeight;
    const double density=s.mass/area;
    if(!std::isfinite(density)||density<=0||density>1e8)return false;
    Remove(key);
    if(!b2World_IsValid(m_world)) {auto d=b2DefaultWorldDef();d.gravity={0,-9.81f};m_world=b2CreateWorld(&d);}
    auto d=b2DefaultBodyDef();
    d.type=s.type==1?b2_dynamicBody:s.type==2?b2_kinematicBody:b2_staticBody;
    d.position={s.x,s.y};d.rotation=b2MakeRot(s.angle);d.gravityScale=s.gravityScale;
    d.linearDamping=s.damping;d.fixedRotation=s.fixedRotation;d.isBullet=s.bullet;
    auto body=b2CreateBody(m_world,&d);
    auto shape=b2DefaultShapeDef();shape.isSensor=s.sensor;
    shape.enableSensorEvents=true;shape.enableContactEvents=true;
    shape.userData=reinterpret_cast<void*>(std::uintptr_t(key));
    shape.filter.categoryBits=s.category;shape.filter.maskBits=s.mask;
    shape.material.friction=s.friction;shape.material.restitution=s.restitution;
    shape.density=static_cast<float>(density);
    b2ShapeId id;
    if(s.circle) {b2Circle circle{{s.offsetX,s.offsetY},s.radius};id=b2CreateCircleShape(body,&shape,&circle);}
    else {auto box=b2MakeOffsetBox(s.halfWidth,s.halfHeight,{s.offsetX,s.offsetY},b2MakeRot(0));id=b2CreatePolygonShape(body,&shape,&box);}
    m_bodies[key]={body,id};
    return true;
}
void Physics2DWorld::Remove(std::uint64_t key) {
    const auto it=m_bodies.find(key);if(it==m_bodies.end()) return;
    for(auto p=m_pairs.begin();p!=m_pairs.end();) {
        if(p->first.first==key||p->first.second==key) {auto event=p->second;event.enter=false;m_events.push_back(event);p=m_pairs.erase(p);}
        else ++p;
    }
    b2DestroyBody(it->second.body);m_bodies.erase(it);
}
void Physics2DWorld::Clear() {
    if(b2World_IsValid(m_world)) b2DestroyWorld(m_world);
    m_world={};m_bodies.clear();m_pairs.clear();m_events.clear();
}
void Physics2DWorld::Begin(b2ShapeId sa,b2ShapeId sb,bool trigger,const b2Manifold* manifold) {
    auto a=ShapeKey(sa),b=ShapeKey(sb);if(!a||!b) return;
    PhysicsContact2D event;event.a=(std::min)(a,b);event.b=(std::max)(a,b);event.trigger=trigger;
    if(manifold) {
        const float sign=a==event.a?1.f:-1.f;
        event.nx=manifold->normal.x*sign;event.ny=manifold->normal.y*sign;
        if(manifold->pointCount) {event.px=manifold->points[0].point.x;event.py=manifold->points[0].point.y;}
    }
    if(m_pairs.emplace(std::make_pair(event.a,event.b),event).second) m_events.push_back(event);
}
void Physics2DWorld::End(b2ShapeId sa,b2ShapeId sb) {
    auto a=ShapeKey(sa),b=ShapeKey(sb);if(!a||!b) return;
    const auto it=m_pairs.find({(std::min)(a,b),(std::max)(a,b)});
    if(it!=m_pairs.end()) {auto event=it->second;event.enter=false;m_events.push_back(event);m_pairs.erase(it);}
}
void Physics2DWorld::Step(float dt) {
    if(!b2World_IsValid(m_world)||!std::isfinite(dt)||dt<=0) return;
    b2World_Step(m_world,dt,4);
    const auto sensors=b2World_GetSensorEvents(m_world);
    for(int i=0;i<sensors.beginCount;++i) Begin(sensors.beginEvents[i].sensorShapeId,sensors.beginEvents[i].visitorShapeId,true);
    for(int i=0;i<sensors.endCount;++i) End(sensors.endEvents[i].sensorShapeId,sensors.endEvents[i].visitorShapeId);
    const auto contacts=b2World_GetContactEvents(m_world);
    for(int i=0;i<contacts.beginCount;++i) Begin(contacts.beginEvents[i].shapeIdA,contacts.beginEvents[i].shapeIdB,false,&contacts.beginEvents[i].manifold);
    for(int i=0;i<contacts.endCount;++i) End(contacts.endEvents[i].shapeIdA,contacts.endEvents[i].shapeIdB);
}
void Physics2DWorld::SetVelocity(std::uint64_t key,float x,float y) {const auto it=m_bodies.find(key);if(it!=m_bodies.end()&&std::isfinite(x)&&std::isfinite(y)&&std::fabs(x)<=100000&&std::fabs(y)<=100000)b2Body_SetLinearVelocity(it->second.body,{x,y});}
void Physics2DWorld::SetTransform(std::uint64_t key,float x,float y,float angle) {const auto it=m_bodies.find(key);if(it!=m_bodies.end()&&std::isfinite(x)&&std::isfinite(y)&&std::isfinite(angle)&&std::fabs(x)<=100000&&std::fabs(y)<=100000)b2Body_SetTransform(it->second.body,{x,y},b2MakeRot(angle));}
bool Physics2DWorld::State(std::uint64_t key,PhysicsBody2DState& state) const {
    const auto it=m_bodies.find(key);if(it==m_bodies.end())return false;
    const auto position=b2Body_GetPosition(it->second.body),velocity=b2Body_GetLinearVelocity(it->second.body);
    state={position.x,position.y,b2Rot_GetAngle(b2Body_GetRotation(it->second.body)),velocity.x,velocity.y};return true;
}
bool Physics2DWorld::Raycast(float x,float y,float dx,float dy,float distance,std::uint32_t mask,bool sensors,PhysicsRay2D& hit) const {
    hit={};const float length=std::sqrt(dx*dx+dy*dy);
    if(!std::isfinite(x)||!std::isfinite(y)||!b2World_IsValid(m_world)||!std::isfinite(length)||length<1e-6f||!std::isfinite(distance)||distance<=0||distance>100000||std::fabs(x)>100000||std::fabs(y)>100000)return false;
    struct Query {bool sensors;PhysicsRay2D hit;float distance;};Query query{sensors,{},distance};
    auto callback=[](b2ShapeId shape,b2Vec2 point,b2Vec2 normal,float fraction,void* context)->float {
        auto& q=*static_cast<Query*>(context);if(!q.sensors&&b2Shape_IsSensor(shape))return -1;
        q.hit={ShapeKey(shape),point.x,point.y,normal.x,normal.y,fraction*q.distance};return fraction;
    };
    auto filter=b2DefaultQueryFilter();filter.maskBits=mask;
    b2World_CastRay(m_world,{x,y},{dx/length*distance,dy/length*distance},filter,callback,&query);
    hit=query.hit;return hit.key!=0;
}
std::vector<PhysicsContact2D> Physics2DWorld::TakeEvents() {auto result=std::move(m_events);m_events.clear();return result;}
