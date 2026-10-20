#include "SamplePhysics2DPlayerScript.h"
#include "engine/input/key_logger.h"
#include "engine/scripting/script_dll_register.h"
#include "engine/scripting/script_runtime_api.h"
#include "engine/ecs/game_object.h"
#include "engine/ui/ui_canvas.h"
#include <cmath>
REGISTER_SCRIPT(SamplePhysics2DPlayerScript)
REGISTER_SCRIPT(SamplePhysics2DCollectibleScript)
void SamplePhysics2DPlayerScript::OnCreate(engine::GameObject&) {
    collected=0;
    completed=false;
    if(targetCount<1)targetCount=1;
    UpdateScore();
}
void SamplePhysics2DPlayerScript::OnDestroy(engine::GameObject&) {
    collected=0;completed=false;UpdateScore();
}
void SamplePhysics2DPlayerScript::UpdateScore() {
    auto* canvas=ScriptApi_SceneCanvasGetCanvas();
    if(!canvas)return;
    const std::string score="Score "+std::to_string(collected)+"/"+std::to_string(targetCount)
        +(completed?" - Complete! N: Next scene | R: Restart":" | R: Restart");
    for(const auto& element:canvas->GetElements()) {
        if(element->GetName()!="Score")continue;
        if(auto* text=dynamic_cast<UIText*>(element.get()))ScriptApi_UITextSetText(*text,score.c_str());
    }
}
void SamplePhysics2DPlayerScript::OnUpdate(engine::GameObject& self,float) {
    UpdateScore();
    if(KeyLogger_IsTrigger(KK_R)&&!sceneFile.empty()) {
        ScriptApi_SceneCanvasRequestSceneFile(sceneFile.c_str(),true);
        return;
    }
    if(completed&&KeyLogger_IsTrigger(KK_N)&&!nextScene.empty()) {
        ScriptApi_SceneCanvasRequestSceneFile(nextScene.c_str(),true);
        return;
    }
    const auto* keys=Keyboard_GetState();
    if(!keys)return;
    float x=float(keys->D||keys->Right)-float(keys->A||keys->Left);
    float y=float(keys->W||keys->Up)-float(keys->S||keys->Down);
    const float length=std::sqrt(x*x+y*y);
    if(length>1) {x/=length;y/=length;}
    ScriptApi_Physics2DSetVelocity(&self,{x*3,y*3});
    ScriptApi_SpritePlay(&self,length>0?"walk":"idle");
    if(x!=0)ScriptApi_SpriteSetFlip(&self,x<0,false);
}
void SamplePhysics2DPlayerScript::OnPhysics2DEvent(GameObject&,const Physics2DEvent& event) {
    if(event.enter&&event.trigger&&event.other&&event.other->IsActive()&&event.other->GetName()=="Collectible") {
        event.other->SetActive(false);
        ++collected;
        completed=collected>=targetCount;
        UpdateScore();

    }
}
