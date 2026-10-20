#include "engine/ecs/sprite_animation.h"
#include <cassert>
#include <limits>

int main() {
    SpriteAnimationClip walk;
    walk.frames.resize(8); walk.fps=8;
    SpriteAnimationClock a,b;
    a.Reset(true); b.Reset(true);
    a.Tick(0.25,walk); assert(a.Frame(walk)==2 && b.Frame(walk)==0);
    a.playing=false; a.Tick(1,walk); assert(a.Frame(walk)==2);
    a.playing=true;a.Tick(1000,walk);assert(a.Frame(walk)==2);
    a.Reset(true);for(int i=0;i<60;++i) a.Tick(1.0/60,walk);
    assert(a.Frame(walk)==0);
    a.Reset(true);walk.loop=false;a.Tick(4,walk);
    assert(!a.playing && a.Frame(walk)==7);
    a.Reset();assert(a.Frame(walk)==0 && !a.playing);
    a.Reset(true);a.Tick(-1,walk);a.Tick(std::numeric_limits<double>::infinity(),walk);
    assert(a.Frame(walk)==0);
    walk.frames.clear();a.Tick(1,walk);assert(a.Frame(walk)==0);
}
