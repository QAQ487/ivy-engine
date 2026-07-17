#pragma once

#include "component.h"

#include <string>

class EffectComponent : public Component {
public:
    std::string effectPath;
    bool autoPlay = true;
    bool autoDestroy = false;
    float speed = 1.0f;
    float lifetimeOverride = -1.0f;  // -1 = use effekseer default
    float randomStartOffset = 0.0f;

    const char* GetTypeName() const override { return "Effect"; }

    void OnAttach() override;
    void OnDetach() override;
    void Update(double elapsed_time) override;
    void OnReset() override;

    std::string Serialize() const override;
    void Deserialize(const std::string& jsonStr) override;

    void Play();
    void Stop();
    bool IsPlaying() const;
    void SetEffectPath(const std::string& path);

private:
    int m_effectId = -1;
    int m_handle = -1;
    bool m_hasAutoPlayed = false;

    bool LoadEffect();
    void SyncTransform();
};
