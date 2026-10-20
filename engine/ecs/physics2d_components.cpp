#include "physics2d_components.h"
#include "engine/physics/2d/physics2d.h"
#include "libraries/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
using json=nlohmann::json;
static float Number(const json& j,const char* key,float fallback,float minimum) {
    const float v=j.value(key,fallback);return std::isfinite(v)?(std::max)(minimum,v):fallback;
}
static DirectX::XMFLOAT2 Vec(const json& j,const char* key,DirectX::XMFLOAT2 fallback) {
    if(!j.contains(key)||!j[key].is_array()||j[key].size()!=2)return fallback;
    const float x=j[key][0].get<float>(),y=j[key][1].get<float>();
    return std::isfinite(x)&&std::isfinite(y)?DirectX::XMFLOAT2{x,y}:fallback;
}
std::string Rigidbody2DComponent::Serialize() const {
    return json{{"bodyType",bodyType==BodyType2D::Static?"static":bodyType==BodyType2D::Kinematic?"kinematic":"dynamic"},
        {"mass",mass},{"gravityScale",gravityScale},{"linearDamping",linearDamping},{"fixedRotation",fixedRotation},
        {"bullet",bullet},{"initialVelocity",{initialVelocity.x,initialVelocity.y}}}.dump();
}
void Rigidbody2DComponent::Deserialize(const std::string& data) {
    const auto j=json::parse(data);const auto type=j.value("bodyType",std::string("dynamic"));
    bodyType=type=="static"?BodyType2D::Static:type=="kinematic"?BodyType2D::Kinematic:BodyType2D::Dynamic;
    mass=Number(j,"mass",1,.001f);gravityScale=Number(j,"gravityScale",0,-100);linearDamping=Number(j,"linearDamping",0,0);
    fixedRotation=j.value("fixedRotation",true);bullet=j.value("bullet",true);
    initialVelocity=Vec(j,"initialVelocity",{});OnReset();
}
void Rigidbody2DComponent::OnReset() {velocity=initialVelocity;hasMoveTarget=false;}
void Rigidbody2DComponent::OnDetach() {Physics2D_Remove(owner);}
bool Rigidbody2DComponent::MovePosition(DirectX::XMFLOAT2 target) {
    if(bodyType!=BodyType2D::Kinematic||!std::isfinite(target.x)||!std::isfinite(target.y))return false;
    moveTarget=target;hasMoveTarget=true;return true;
}
std::string Collider2DComponent::Serialize() const {
    return json{{"shape",shape==Shape2D::Circle?"circle":"box"},{"size",{size.x,size.y}},{"offset",{offset.x,offset.y}},
        {"radius",radius},{"friction",friction},{"restitution",restitution},{"isTrigger",isTrigger},
        {"enabled",enabled},{"showDebugCollider",showDebugCollider},{"categoryBits",categoryBits},{"maskBits",maskBits}}.dump();
}
void Collider2DComponent::Deserialize(const std::string& data) {
    const auto j=json::parse(data);shape=j.value("shape",std::string("box"))=="circle"?Shape2D::Circle:Shape2D::Box;
    size=Vec(j,"size",{1,1});size.x=(std::max)(.001f,size.x);size.y=(std::max)(.001f,size.y);offset=Vec(j,"offset",{});
    radius=Number(j,"radius",.5f,.001f);friction=Number(j,"friction",.3f,0);
    restitution=(std::min)(1.f,Number(j,"restitution",0,0));isTrigger=j.value("isTrigger",false);enabled=j.value("enabled",true);
    showDebugCollider=j.value("showDebugCollider",false);
    categoryBits=j.value("categoryBits",std::uint32_t(1));maskBits=j.value("maskBits",std::uint32_t(0xffffffff));
}
void Collider2DComponent::OnDetach() {Physics2D_Remove(owner);}
