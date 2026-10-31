// sprite_animator_component.h
// ECS component that drives sprite animations.

#pragma once
#include "component.h"
#include "sprite_animation.h"
#include <map>

class SpriteAnimatorComponent : public Component {
public:
    std::map<std::string,SpriteAnimationClip> clips;
    std::string defaultClip;
    bool autoPlay=true;
    const char* GetTypeName() const override {return "SpriteAnimator";}
    std::string Serialize() const override;
    void Deserialize(const std::string&) override;
    void OnAttach() override {OnReset();}
    void OnReset() override;
    void Update(double dt) override;
    bool Play(const std::string& name,bool restart=false);
    void Pause() {m_clock.playing=false;}
    void Stop() {m_clock.Reset();}
    const SpriteAnimationClip* CurrentClip() const;
    const SpriteAnimationFrame* CurrentFrame() const;
    const std::string& ActiveClip() const {return m_active;}
    bool IsPlaying() const {return m_clock.playing;}
    bool ImportSheet(const std::string& metadataPath,const std::string& clipName);
private:
    std::string m_active;
    SpriteAnimationClock m_clock;
};
