#include "engine/input/key_logger.h"
#include "SampleSpritePlayerScript.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include <cmath>

REGISTER_SCRIPT(SampleSpritePlayerScript)
void SampleSpritePlayerScript::OnUpdate(engine::GameObject& self,float dt) {
    const auto* keys=Keyboard_GetState();
    if(!keys) return;
    float x=float(keys->D||keys->Right)-float(keys->A||keys->Left);
    float y=float(keys->W||keys->Up)-float(keys->S||keys->Down);
    const bool moving=x!=0||y!=0;
    if(KeyLogger_IsTrigger(KK_P)) {paused=!paused;stopped=false;}
    if(KeyLogger_IsTrigger(KK_O)) {stopped=true;paused=false;}
    if(paused) {ScriptApi_SpritePause(&self);return;}
    if(stopped && !moving) {ScriptApi_SpriteStop(&self);return;}
    if(moving) stopped=false;
    const float length=std::sqrt(x*x+y*y);
    if(length>1) {x/=length;y/=length;}
    if(auto* transform=ScriptApi_FindTransform(self)) {
        transform->localPosition.x+=x*moveSpeed*dt;
        transform->localPosition.y+=y*moveSpeed*dt;
    }
    ScriptApi_SpritePlay(&self,moving?"walk":"idle");
    if(x!=0) ScriptApi_SpriteSetFlip(&self,x<0,false);

}
