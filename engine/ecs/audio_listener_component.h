#pragma once

#include "component.h"

#include <string>

class AudioListenerComponent : public Component {
public:
    const char* GetTypeName() const override { return "AudioListener"; }

    std::string Serialize() const override;
    void Deserialize(const std::string& jsonStr) override;
};
