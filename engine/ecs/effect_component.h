#pragma once

#include "component.h"

#include <string>
#include <DirectXMath.h>

class EffectComponent : public Component {
public:
    std::string effectPath;
    bool autoPlay = true;
    bool autoDestroy = false;
    float speed = 1.0f;
    float lifetimeOverride = -1.0f;
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

    // Main thread only.
    void FlushCommands();

private:
    int m_effectId = -1;
    int m_handle = -1;
    bool m_hasAutoPlayed = false;

    bool m_cmdPlay = false;
    bool m_cmdStop = false;
    bool m_cmdSync = false;
    bool m_cmdCheckExists = false;
    bool m_pendingAutoDestroy = false;

    bool LoadEffect();
    void SyncTransformImmediate();
    void ApplyStopImmediate();
};
