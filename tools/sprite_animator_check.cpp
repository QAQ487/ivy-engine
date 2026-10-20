#include "engine/ecs/sprite_animator_component.h"
#include <cassert>

static bool playing=true;
bool EngineGUI_IsPlaying() {return playing;}
int main() {
    const std::string data=R"({"defaultClip":"idle","autoPlay":true,"clips":{
        "idle":{"texture":"rom/idle.png","fps":1,"frames":[[0,0,64,64]]},
        "walk":{"texture":"rom/walk.png","fps":4,"loop":true,"frames":[[0,0,64,64],[64,0,64,64],[128,0,64,64]]}}})";
    SpriteAnimatorComponent a,b;
    a.Deserialize(data);b.Deserialize(data);
    const auto authored=a.Serialize();
    assert(a.Play("walk"));a.Update(.25);
    assert(a.CurrentFrame()->x==64 && b.CurrentFrame()->x==0);
    assert(a.Play("walk"));a.Update(.25);assert(a.CurrentFrame()->x==128);
    a.Pause();a.Update(1);assert(a.CurrentFrame()->x==128);
    a.Play("walk");a.Update(.25);assert(a.CurrentFrame()->x==0);
    a.Update(.25);a.Stop();assert(!a.IsPlaying() && a.CurrentFrame()->x==0);
    assert(!a.Play("missing"));
    a.Play("walk",true);a.Update(.25);
    playing=false;a.Update(1);assert(a.CurrentFrame()->x==64);
    assert(a.Serialize()==authored); // Runtime state is never serialized as authoring data.
    a.OnReset();assert(a.ActiveClip()=="idle" && a.CurrentFrame()->x==0 && a.IsPlaying());
    SpriteAnimatorComponent roundTrip;roundTrip.Deserialize(authored);
    assert(roundTrip.Serialize()==authored);
}
