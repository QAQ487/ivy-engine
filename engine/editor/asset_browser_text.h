// asset_browser_text.h
// Pure helpers for Asset Browser tile/list labels: UTF-8-safe ellipsis and
// word/character wrap. Header-only so Linux unit tests compile without ImGui.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace asset_browser_text {

// Pixel-width measure. `text` is a UTF-8 substring (may be non-null-terminated).
using MeasureFn = float (*)(std::string_view text, void* user);

// Byte length of the next UTF-8 codepoint at `s`, or 0 if empty / truncated /
// invalid lead that cannot form a complete sequence within `nbytes`.
inline std::size_t Utf8CodepointByteLength(const char* s, std::size_t nbytes) {
    if (!s || nbytes == 0)
        return 0;
    const unsigned char c = static_cast<unsigned char>(s[0]);
    std::size_t need = 1;
    if (c < 0x80u)
        need = 1;
    else if ((c & 0xE0u) == 0xC0u)
        need = 2;
    else if ((c & 0xF0u) == 0xE0u)
        need = 3;
    else if ((c & 0xF8u) == 0xF0u)
        need = 4;
    else
        return 0; // unexpected continuation / invalid lead

    if (need > nbytes)
        return 0;
    for (std::size_t i = 1; i < need; ++i) {
        const unsigned char cc = static_cast<unsigned char>(s[i]);
        if ((cc & 0xC0u) != 0x80u)
            return 0;
    }
    return need;
}

inline float MeasureOrZero(MeasureFn measure, void* user, std::string_view text) {
    if (!measure)
        return 0.0f;
    return measure(text, user);
}

// Longest UTF-8 prefix of `text` whose measured width is <= maxWidth.
inline std::string_view Utf8PrefixFitting(
    std::string_view text, float maxWidth, MeasureFn measure, void* user)
{
    if (text.empty() || maxWidth < 0.0f)
        return {};
    if (MeasureOrZero(measure, user, text) <= maxWidth)
        return text;

    std::size_t fit = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        const std::size_t n = Utf8CodepointByteLength(text.data() + i, text.size() - i);
        if (n == 0)
            break;
        const std::string_view cand = text.substr(0, i + n);
        if (MeasureOrZero(measure, user, cand) > maxWidth)
            break;
        fit = i + n;
        i += n;
    }
    return text.substr(0, fit);
}

// Truncate with "..." so the result fits maxWidth. Never splits a multibyte char.
inline std::string Ellipsize(
    std::string_view text, float maxWidth, MeasureFn measure, void* user)
{
    if (text.empty())
        return {};
    if (maxWidth <= 0.0f)
        return {};
    if (MeasureOrZero(measure, user, text) <= maxWidth)
        return std::string(text);

    constexpr std::string_view kEllipsis = "...";
    const float ellipsisW = MeasureOrZero(measure, user, kEllipsis);
    if (ellipsisW > maxWidth)
        return {};
    if (ellipsisW == maxWidth)
        return std::string(kEllipsis);

    const float budget = maxWidth - ellipsisW;
    const std::string_view prefix = Utf8PrefixFitting(text, budget, measure, user);
    std::string out;
    out.reserve(prefix.size() + kEllipsis.size());
    out.append(prefix);
    out.append(kEllipsis);
    return out;
}

// Prefer breaking after the last ASCII space in `chunk` when the full chunk
// does not fit; otherwise break at a UTF-8 codepoint boundary.
inline std::string_view FitOneLine(
    std::string_view text, float maxWidth, MeasureFn measure, void* user)
{
    if (text.empty())
        return {};
    if (MeasureOrZero(measure, user, text) <= maxWidth)
        return text;

    const std::string_view hard = Utf8PrefixFitting(text, maxWidth, measure, user);
    if (hard.empty())
        return {};

    // Soft wrap: last space within the hard prefix (not at position 0).
    const std::size_t space = hard.find_last_of(' ');
    if (space != std::string_view::npos && space > 0)
        return text.substr(0, space);
    return hard;
}

inline std::string_view SkipLeadingAsciiSpaces(std::string_view text) {
    std::size_t i = 0;
    while (i < text.size() && text[i] == ' ')
        ++i;
    return text.substr(i);
}

// Wrap to at most maxLines within maxWidth. The final line uses Ellipsize when
// remaining text still overflows. Lines are joined with '\n'.
inline std::string WrapEllipsize(
    std::string_view text,
    float maxWidth,
    int maxLines,
    MeasureFn measure,
    void* user)
{
    if (text.empty() || maxLines <= 0 || maxWidth <= 0.0f)
        return {};

    if (maxLines == 1)
        return Ellipsize(text, maxWidth, measure, user);

    std::string result;
    std::string_view remaining = text;
    int linesEmitted = 0;

    while (!remaining.empty() && linesEmitted < maxLines) {
        const bool lastLine = (linesEmitted == maxLines - 1);
        if (lastLine) {
            const std::string line = Ellipsize(remaining, maxWidth, measure, user);
            if (!result.empty())
                result.push_back('\n');
            result.append(line);
            break;
        }

        std::string_view take = FitOneLine(remaining, maxWidth, measure, user);
        if (take.empty()) {
            // Nothing fits on a non-final line (extreme narrow width): fall through
            // to ellipsize on the last line budget.
            const std::string line = Ellipsize(remaining, maxWidth, measure, user);
            if (!result.empty())
                result.push_back('\n');
            result.append(line);
            break;
        }

        if (!result.empty())
            result.push_back('\n');
        result.append(take);
        ++linesEmitted;

        remaining = remaining.substr(take.size());
        remaining = SkipLeadingAsciiSpaces(remaining);

        if (remaining.empty())
            break;

        // If the whole remainder fits on the next (still non-forced) lines as one
        // chunk and this was the only break needed, loop continues normally.
    }

    return result;
}

} // namespace asset_browser_text
