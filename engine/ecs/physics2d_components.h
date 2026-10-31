// physics2d_components.h
// ECS components for 2D physics bodies/colliders.

#pragma once
#include "component.h"
#include <DirectXMath.h>
#include <cstdint>

enum class BodyType2D {Static,Dynamic,Kinematic};
enum class Shape2D {Box,Circle};
class Rigidbody2DComponent : public Component {
public:
    BodyType2D bodyType=BodyType2D::Dynamic;
    float mass=1,gravityScale=0,linearDamping=0;
    bool fixedRotation=true,bullet=true;
    DirectX::XMFLOAT2 initialVelocity{},velocity{},moveTarget{};
    bool hasMoveTarget=false;
    const char* GetTypeName() const override {return "Rigidbody2D";}
    std::string Serialize() const override;
    void Deserialize(const std::string&) override;
    void OnAttach() override {OnReset();}
    void OnReset() override;
    void OnDetach() override;
    bool MovePosition(DirectX::XMFLOAT2 target);
};
class Collider2DComponent : public Component {
public:
    Shape2D shape=Shape2D::Box;
    DirectX::XMFLOAT2 size{1,1},offset{};
    float radius=.5f,friction=.3f,restitution=0;
    bool isTrigger=false,enabled=true,showDebugCollider=false;
    std::uint32_t categoryBits=1,maskBits=0xffffffff;
    const char* GetTypeName() const override {return "Collider2D";}
    std::string Serialize() const override;
    void Deserialize(const std::string&) override;
    void OnDetach() override;
};
