// sprite_animator_component.cpp
// ECS component that drives sprite animations.

#include "sprite_animator_component.h"
#include "libraries/nlohmann/json.hpp"
#include "engine/core/engine_gui.h"
#include <filesystem>
#include <fstream>

using json=nlohmann::json;
std::string SpriteAnimatorComponent::Serialize() const {
    json j={{"defaultClip",defaultClip},{"autoPlay",autoPlay},{"clips",json::object()}};
    for(const auto& [name,clip]:clips) {
        json frames=json::array();
        for(const auto& f:clip.frames) frames.push_back({f.x,f.y,f.w,f.h});
        j["clips"][name]={{"texture",clip.texture},{"fps",clip.fps},{"loop",clip.loop},{"frames",frames}};
    }
    return j.dump();
}
void SpriteAnimatorComponent::Deserialize(const std::string& text) {
    const auto j=json::parse(text);
    clips.clear(); defaultClip=j.value("defaultClip",std::string{}); autoPlay=j.value("autoPlay",true);
    if(j.contains("clips") && j["clips"].is_object()) for(auto it=j["clips"].begin();it!=j["clips"].end();++it) {
        if(!it.value().is_object()) continue;
        SpriteAnimationClip clip;
        clip.texture=it.value().value("texture",std::string{});
        clip.fps=it.value().value("fps",10.0); clip.loop=it.value().value("loop",true);
        if(!std::isfinite(clip.fps) || clip.fps<=0) clip.fps=10;
        if(it.value().contains("frames") && it.value()["frames"].is_array()) for(const auto& f:it.value()["frames"]) {
            if(!f.is_array() || f.size()!=4) continue;
            SpriteAnimationFrame frame{f[0].get<float>(),f[1].get<float>(),f[2].get<float>(),f[3].get<float>()};
            if(std::isfinite(frame.x)&&std::isfinite(frame.y)&&std::isfinite(frame.w)&&std::isfinite(frame.h)
                &&frame.x>=0&&frame.y>=0&&frame.w>0&&frame.h>0) clip.frames.push_back(frame);
        }
        clips.emplace(it.key(),std::move(clip));
    }
    OnReset();
}
void SpriteAnimatorComponent::OnReset() {m_active=defaultClip;m_clock.Reset(autoPlay);}
bool SpriteAnimatorComponent::Play(const std::string& name,bool restart) {
    const auto it=clips.find(name);
    if(it==clips.end() || it->second.frames.empty()) return false;
    if(m_active!=name || restart) m_clock.Reset();
    m_active=name;m_clock.playing=true;return true;
}
const SpriteAnimationClip* SpriteAnimatorComponent::CurrentClip() const {
    const auto it=clips.find(m_active.empty()?defaultClip:m_active);
    return it==clips.end()?nullptr:&it->second;
}
const SpriteAnimationFrame* SpriteAnimatorComponent::CurrentFrame() const {
    const auto* clip=CurrentClip();
    return !clip || clip->frames.empty()?nullptr:&clip->frames[m_clock.Frame(*clip)];
}
void SpriteAnimatorComponent::Update(double dt) {
    if(!EngineGUI_IsPlaying()) return;
    if(const auto* clip=CurrentClip()) m_clock.Tick(dt,*clip);
}
bool SpriteAnimatorComponent::ImportSheet(const std::string& metadataPath,const std::string& name) {
    if(name.empty()) return false;
    try {
        const auto path=std::filesystem::u8path(metadataPath);
        std::ifstream file(path); if(!file) return false;
        json j;file>>j;
        SpriteAnimationClip clip;
        const auto utf8=(path.parent_path()/std::filesystem::u8path(j.at("image").get<std::string>())).generic_u8string();
        clip.texture.assign(reinterpret_cast<const char*>(utf8.data()),utf8.size());
        for(const auto& f:j.at("frames")) {
            SpriteAnimationFrame frame{f.at("x").get<float>(),f.at("y").get<float>(),f.at("w").get<float>(),f.at("h").get<float>()};
            if(frame.x<0||frame.y<0||frame.w<=0||frame.h<=0) return false;
            clip.frames.push_back(frame);
        }
        if(clip.frames.empty()) return false;
        clips[name]=std::move(clip); if(defaultClip.empty()) defaultClip=name;
        OnReset();return true;
    } catch(...) {return false;}
}
