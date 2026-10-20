#include "engine/physics/2d/physics2d_world.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

static void Check(bool condition,const char* message) {
    if(!condition) {std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
static constexpr float Step=1.f/60.f;
static PhysicsBody2DState Simulate(int fps) {
    Physics2DWorld world;PhysicsBody2DSpec body;body.type=1;body.gravityScale=1;
    world.Add(1,body);world.SetVelocity(1,3,0);
    double accumulator=0;int ticks=0;
    for(int frame=0;frame<fps*2;++frame) {
        accumulator+=1.0/fps;
        while(accumulator+1e-9>=1.0/60.0) {world.Step(Step);accumulator-=1.0/60.0;++ticks;}
    }
    Check(ticks==120,"fixed step count differs with render FPS");
    PhysicsBody2DState state;Check(world.State(1,state),"dynamic body missing");return state;
}
static void FrameRates() {
    const auto reference=Simulate(60);
    for(int fps:{30,144}) {
        const auto state=Simulate(fps);
        Check(std::fabs(state.x-reference.x)<.0001f&&std::fabs(state.y-reference.y)<.0001f,
              "trajectory differs at 30/60/144 render FPS");
    }
    Check(reference.x>5.9f&&reference.y<-19,"simulation did not advance velocity/gravity");
}
static void Wall(bool circle) {
    Physics2DWorld world;PhysicsBody2DSpec wall;wall.x=2;wall.halfWidth=.25f;wall.halfHeight=5;
    world.Add(1,wall);PhysicsBody2DSpec body;body.type=1;body.circle=circle;
    world.Add(2,body);
    bool enter=false;
    for(int i=0;i<120;++i) {
        world.SetVelocity(2,8,0);world.Step(Step);
        for(const auto& event:world.TakeEvents()) if(event.enter&&!event.trigger)enter=true;
    }
    PhysicsBody2DState state;Check(world.State(2,state),"wall test body missing");
    Check(state.x>1.1f&&state.x<1.35f,"box/circle passed through wall");
    Check(enter,"wall collision did not emit contact enter");
}
static bool Has(const std::vector<PhysicsContact2D>& events,bool enter) {
    for(const auto& event:events) if(event.a==1&&event.b==2&&event.trigger&&event.enter==enter)return true;
    return false;
}
static void Triggers() {
    Physics2DWorld world;PhysicsBody2DSpec sensor;sensor.sensor=true;world.Add(1,sensor);
    PhysicsBody2DSpec visitor;visitor.type=1;visitor.circle=true;world.Add(2,visitor);
    world.Step(Step);Check(Has(world.TakeEvents(),true),"trigger enter missing");
    world.SetTransform(2,5,0,0);world.Step(Step);
    Check(Has(world.TakeEvents(),false),"trigger exit missing");
    world.SetTransform(2,0,0,0);world.Step(Step);
    Check(Has(world.TakeEvents(),true),"trigger re-enter missing");
    world.Remove(2);Check(Has(world.TakeEvents(),false),"Remove did not synthesize trigger exit");
    PhysicsBody2DState state;Check(!world.State(2,state),"removed body still exists");
    world.Step(Step);Check(world.TakeEvents().empty(),"removed pair emitted duplicate exit");
}
static void FiltersAndRaycast() {
    Physics2DWorld world;PhysicsBody2DSpec sensor;sensor.x=2;sensor.sensor=true;sensor.category=2;
    world.Add(1,sensor);PhysicsBody2DSpec solid;solid.x=4;solid.category=4;world.Add(2,solid);
    PhysicsRay2D hit;
    Check(world.Raycast(0,0,1,0,10,0xffffffff,false,hit)&&hit.key==2,"ray did not ignore sensor");
    Check(std::fabs(hit.distance-3.5f)<.001f,"ray distance incorrect");
    Check(world.Raycast(0,0,1,0,10,0xffffffff,true,hit)&&hit.key==1,"ray did not include sensor");
    Check(world.Raycast(0,0,1,0,10,4,true,hit)&&hit.key==2,"ray category mask ignored");
    Check(!world.Raycast(0,0,1,0,10,8,true,hit),"ray hit excluded category");
    Check(!world.Raycast(0,0,0,0,10,~0u,true,hit),"zero ray direction accepted");
    world.Clear();Check(!world.Raycast(0,0,1,0,10,~0u,true,hit),"Clear retained raycast shapes");
    Check(world.TakeEvents().empty(),"Clear retained events");
    PhysicsBody2DSpec body;body.type=1;body.mask=0;body.x=4;world.Add(3,body);world.Add(2,solid);
    for(int i=0;i<3;++i)world.Step(Step);
    Check(world.TakeEvents().empty(),"collision mask did not exclude contact");
    PhysicsBody2DState state;Check(world.State(3,state),"world could not recreate after Clear");
}
int main() {
    Physics2DWorld invalidWorld;PhysicsBody2DSpec invalid;invalid.mass=1e38f;
    Check(!invalidWorld.Add(9,invalid),"overflowing mass accepted");
    invalid.mass=1;invalid.radius=1e38f;
    Check(!invalidWorld.Add(9,invalid),"overflowing geometry accepted");
    invalid.radius=.000001f;invalid.circle=true;
    Check(!invalidWorld.Add(9,invalid),"excessive density accepted");
    FrameRates();Wall(false);Wall(true);Triggers();FiltersAndRaycast();
    std::puts("PASS: Physics2D FPS, box/circle contacts, triggers, removal, filters, raycast, reset");return 0;
}
