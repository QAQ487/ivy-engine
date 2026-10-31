// controller_inject_protocol.h
// Declares ControllerCommand.

#pragma once

// Controller verb parsing and the one-at-a-time prepare / hold / release
// lifecycle. No Win32 types. The runtime bridge stores one ControllerOperation.

#include "engine/input/controller_test_inject.h"
#include "libraries/nlohmann/json.hpp"

#include <cmath>
#include <cstdint>
#include <string>

namespace ivy::testing {

struct ControllerCommand {
    ControllerInjectSample sample;
    int updates = 0;
};

enum class ControllerParseStatus { Ok, Invalid };

inline bool ReadStick(const nlohmann::json& value, float& out) {
    if (value.is_boolean() || !value.is_number()) return false;
    const double number = value.get<double>();
    if (!std::isfinite(number) || number < -1.0 || number > 1.0) return false;
    out = static_cast<float>(number);
    return true;
}

inline bool ReadTriggerAxis(const nlohmann::json& value, float& out) {
    if (value.is_boolean() || !value.is_number()) return false;
    const double number = value.get<double>();
    if (!std::isfinite(number) || number < 0.0 || number > 1.0) return false;
    out = static_cast<float>(number);
    return true;
}

inline ControllerParseStatus ParseControllerCommand(const nlohmann::json& object, ControllerCommand& out) {
    if (!object.is_object()) return ControllerParseStatus::Invalid;
    static const char* kAllowed[] = {
        "protocolVersion", "requestId", "runId", "verb",
        "lx", "ly", "rx", "ry", "lt", "rt", "buttons", "updates"
    };
    for (auto it = object.begin(); it != object.end(); ++it) {
        bool allowed = false;
        for (const char* name : kAllowed) {
            if (it.key() == name) {
                allowed = true;
                break;
            }
        }
        if (!allowed) return ControllerParseStatus::Invalid;
    }
    if (!object.contains("lx") || !object.contains("ly") || !object.contains("updates"))
        return ControllerParseStatus::Invalid;

    ControllerCommand command;
    try {
        if (!ReadStick(object.at("lx"), command.sample.lx)) return ControllerParseStatus::Invalid;
        if (!ReadStick(object.at("ly"), command.sample.ly)) return ControllerParseStatus::Invalid;
        if (object.contains("rx") && !ReadStick(object.at("rx"), command.sample.rx))
            return ControllerParseStatus::Invalid;
        if (object.contains("ry") && !ReadStick(object.at("ry"), command.sample.ry))
            return ControllerParseStatus::Invalid;
        if (object.contains("lt") && !ReadTriggerAxis(object.at("lt"), command.sample.lt))
            return ControllerParseStatus::Invalid;
        if (object.contains("rt") && !ReadTriggerAxis(object.at("rt"), command.sample.rt))
            return ControllerParseStatus::Invalid;
        if (object.contains("buttons")) {
            const auto& buttons = object.at("buttons");
            if (!buttons.is_array() || buttons.size() > 14) return ControllerParseStatus::Invalid;
            std::uint16_t mask = 0;
            for (const auto& item : buttons) {
                if (!item.is_string()) return ControllerParseStatus::Invalid;
                std::uint16_t bit = 0;
                if (!ControllerButtonFromName(item.get<std::string>(), bit)) return ControllerParseStatus::Invalid;
                if ((mask & bit) != 0) return ControllerParseStatus::Invalid;
                mask = static_cast<std::uint16_t>(mask | bit);
            }
            command.sample.buttons = mask;
        }
        const auto& updates = object.at("updates");
        if (updates.is_boolean() || !updates.is_number_integer()) return ControllerParseStatus::Invalid;
        const int count = updates.get<int>();
        if (count < 1 || count > 120) return ControllerParseStatus::Invalid;
        command.updates = count;
    } catch (const std::exception&) {
        return ControllerParseStatus::Invalid;
    }
    out = command;
    return ControllerParseStatus::Ok;
}

inline ControllerParseStatus ParseControllerCommand(const std::string& json, ControllerCommand& out) {
    const auto object = nlohmann::json::parse(json, nullptr, false);
    if (object.is_discarded()) return ControllerParseStatus::Invalid;
    return ParseControllerCommand(object, out);
}

// Same prepare / hold / release counting as the runtime bridge controller verb.
// Sample() is the injected value for the frame that just began: neutral while
// preparing or releasing, and the queued command while active.
class ControllerOperation {
public:
    enum class State { Idle, Queued, Preparing, Active, Releasing, Completed, Cancelled };

    bool TryQueue(std::int64_t requestId, const ControllerCommand& command) {
        if (state_ == State::Queued || Isolating()) return false;
        state_ = State::Queued;
        cancel_ = false;
        requestId_ = requestId;
        command_ = command;
        remaining_ = command.updates;
        return true;
    }

    void Cancel() {
        if (state_ == State::Queued) {
            state_ = State::Cancelled;
            remaining_ = 0;
            command_ = {};
            cancel_ = false;
        } else if (Isolating()) {
            cancel_ = true;
        }
    }

    void Begin(bool playing) {
        if (!playing) Cancel();
        if (state_ == State::Queued) state_ = State::Preparing;
        if (state_ == State::Active && (cancel_ || remaining_ == 0)) {
            state_ = State::Releasing;
            remaining_ = 0;
            command_.sample = {};
            command_.updates = 0;
        }
    }

    void End() {
        if (state_ == State::Preparing) {
            state_ = State::Active;
        } else if (state_ == State::Active && remaining_ > 0) {
            --remaining_;
        } else if (state_ == State::Releasing) {
            state_ = cancel_ ? State::Cancelled : State::Completed;
            cancel_ = false;
        }
    }

    bool Isolating() const {
        return state_ == State::Preparing || state_ == State::Active || state_ == State::Releasing;
    }

    ControllerInjectSample Sample() const {
        if (state_ == State::Active) return command_.sample;
        return {};
    }

    const char* Status() const {
        switch (state_) {
        case State::Queued:
        case State::Preparing: return "queued";
        case State::Active: return "active";
        case State::Releasing: return "releasing";
        case State::Completed: return "completed";
        case State::Cancelled: return "cancelled";
        default: return "idle";
        }
    }

    std::int64_t RequestId() const { return requestId_; }
    int Remaining() const { return remaining_; }
    bool CancelRequested() const { return cancel_; }

private:
    State state_ = State::Idle;
    bool cancel_ = false;
    std::int64_t requestId_ = 0;
    int remaining_ = 0;
    ControllerCommand command_{};
};

}  // namespace ivy::testing
