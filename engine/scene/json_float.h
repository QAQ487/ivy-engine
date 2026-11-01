// json_float.h
// Shared scene/prefab JSON float formatting (save-time only) and dirty helpers.
// Header-only so Linux unit tests can compile without Windows.

#pragma once

#include "libraries/nlohmann/json.hpp"

#include <cmath>
#include <cstdio>
#include <string>

inline constexpr int kJsonFloatDecimals = 3;

// Format a float for JSON text: round to at most `decimals` places, trim
// trailing zeros, normalize -0 to 0. Non-finite values become "null".
inline std::string JsonFloat_Format(double value, int decimals = kJsonFloatDecimals) {
    if (!std::isfinite(value))
        return "null";
    if (decimals < 0)
        decimals = 0;
    if (decimals > 15)
        decimals = 15;

    double scale = 1.0;
    for (int i = 0; i < decimals; ++i)
        scale *= 10.0;

    double rounded = std::round(value * scale) / scale;
    if (rounded == 0.0)
        rounded = 0.0; // normalize -0

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, rounded);

    // Trim trailing zeros and decimal point.
    std::string s(buf);
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0')
            s.pop_back();
        if (!s.empty() && s.back() == '.')
            s.pop_back();
    }
    if (s == "-0")
        s = "0";
    return s;
}

inline bool JsonFloat_Equal(double a, double b, int decimals = kJsonFloatDecimals) {
    return JsonFloat_Format(a, decimals) == JsonFloat_Format(b, decimals);
}

inline bool JsonFloat_Changed(double before, double after, int decimals = kJsonFloatDecimals) {
    return !JsonFloat_Equal(before, after, decimals);
}

// before/after are pos(3) + rot(3) + scale(3).
inline bool JsonFloat_TransformChanged(const float* before9, const float* after9,
    int decimals = kJsonFloatDecimals) {
    if (!before9 || !after9) return before9 != after9;
    for (int i = 0; i < 9; ++i) {
        if (JsonFloat_Changed(before9[i], after9[i], decimals))
            return true;
    }
    return false;
}

namespace json_float_detail {

inline void DumpValue(const nlohmann::json& j, int indent, int depth, std::string& out) {
    using nlohmann::json;
    switch (j.type()) {
    case json::value_t::null:
        out += "null";
        break;
    case json::value_t::boolean:
        out += j.get<bool>() ? "true" : "false";
        break;
    case json::value_t::number_integer:
        out += std::to_string(j.get<json::number_integer_t>());
        break;
    case json::value_t::number_unsigned:
        out += std::to_string(j.get<json::number_unsigned_t>());
        break;
    case json::value_t::number_float:
        out += JsonFloat_Format(j.get<json::number_float_t>());
        break;
    case json::value_t::string:
        out += j.dump(); // quoted/escaped via nlohmann
        break;
    case json::value_t::array: {
        if (j.empty()) {
            out += "[]";
            break;
        }
        out += '[';
        if (indent >= 0) out += '\n';
        const std::string pad((indent > 0) ? static_cast<size_t>((depth + 1) * indent) : 0u, ' ');
        const std::string padClose((indent > 0) ? static_cast<size_t>(depth * indent) : 0u, ' ');
        for (size_t i = 0; i < j.size(); ++i) {
            if (indent >= 0) out += pad;
            DumpValue(j[i], indent, depth + 1, out);
            if (i + 1 < j.size()) out += ',';
            if (indent >= 0) out += '\n';
        }
        if (indent >= 0) out += padClose;
        out += ']';
        break;
    }
    case json::value_t::object: {
        if (j.empty()) {
            out += "{}";
            break;
        }
        out += '{';
        if (indent >= 0) out += '\n';
        const std::string pad((indent > 0) ? static_cast<size_t>((depth + 1) * indent) : 0u, ' ');
        const std::string padClose((indent > 0) ? static_cast<size_t>(depth * indent) : 0u, ' ');
        size_t i = 0;
        const size_t n = j.size();
        for (auto it = j.begin(); it != j.end(); ++it, ++i) {
            if (indent >= 0) out += pad;
            out += json(it.key()).dump();
            out += (indent >= 0) ? ": " : ":";
            DumpValue(it.value(), indent, depth + 1, out);
            if (i + 1 < n) out += ',';
            if (indent >= 0) out += '\n';
        }
        if (indent >= 0) out += padClose;
        out += '}';
        break;
    }
    case json::value_t::binary:
    case json::value_t::discarded:
        out += "null";
        break;
    }
}

} // namespace json_float_detail

// Pretty/compact dump with rounded floats. indent < 0 => compact; else spaces/level.
inline std::string JsonFloat_Dump(const nlohmann::json& j, int indent = -1) {
    std::string out;
    out.reserve(256);
    json_float_detail::DumpValue(j, indent, 0, out);
    return out;
}

// True when JSON texts match after float rounding (used for dirty/no-op checks).
inline bool JsonFloat_JsonEqual(const std::string& a, const std::string& b) {
    if (a == b) return true;
    try {
        return JsonFloat_Dump(nlohmann::json::parse(a)) ==
               JsonFloat_Dump(nlohmann::json::parse(b));
    } catch (...) {
        return false;
    }
}
