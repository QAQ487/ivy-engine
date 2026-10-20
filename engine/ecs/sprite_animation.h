#pragma once
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

struct SpriteAnimationFrame { float x=0,y=0,w=64,h=64; };
struct SpriteAnimationClip {
    std::string texture;
    std::vector<SpriteAnimationFrame> frames;
    double fps=10;
    bool loop=true;
};
struct SpriteAnimationClock {
    double seconds=0;
    bool playing=false;
    void Reset(bool play=false) { seconds=0; playing=play; }
    std::size_t Frame(const SpriteAnimationClip& clip) const {
        if(clip.frames.empty() || !std::isfinite(clip.fps) || clip.fps<=0) return 0;
        const double count=double(clip.frames.size());
        double frame=std::floor(seconds*clip.fps+1e-9);
        if(clip.loop) frame=std::fmod(frame,count);
        if(frame>=count) frame=count-1;
        return std::size_t(frame);
    }
    void Tick(double dt,const SpriteAnimationClip& clip) {
        if(!playing || !std::isfinite(dt) || dt<=0 || clip.frames.empty()
            || !std::isfinite(clip.fps) || clip.fps<=0) return;
        const double duration=double(clip.frames.size())/clip.fps;
        if(clip.loop) seconds=std::fmod(seconds+std::fmod(dt,duration),duration);
        else {
            seconds+=dt;
            if(seconds>=duration) {seconds=duration;playing=false;}
        }
    }
};
