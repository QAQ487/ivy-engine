// wav_parse.h
// Pure logic for reading a RIFF/WAVE file that is already in memory: finds the
// "fmt " and "data" chunks. AudioManager::LoadWav reads the file bytes through
// a UTF-8 safe path and then uses this, so it no longer needs mmioOpenA (which
// decodes the path as the ANSI code page and fails on CJK file names).
// No Windows or XAudio2 dependency, so it is unit-testable on Linux.

#pragma once

#include <cstddef>
#include <cstdint>

namespace wav_parse {

struct WavChunks {
    std::size_t fmtOffset = 0;   // byte offset of the fmt chunk body
    std::size_t fmtSize = 0;     // fmt chunk body size
    std::size_t dataOffset = 0;  // byte offset of the sample data
    std::size_t dataSize = 0;    // sample data size (clamped to the bytes present)
};

namespace detail {

inline std::uint32_t ReadU32LE(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

inline bool TagEquals(const std::uint8_t* p, const char* tag) {
    return p[0] == static_cast<std::uint8_t>(tag[0]) && p[1] == static_cast<std::uint8_t>(tag[1]) &&
           p[2] == static_cast<std::uint8_t>(tag[2]) && p[3] == static_cast<std::uint8_t>(tag[3]);
}

}  // namespace detail

// Smallest fmt body that holds a PCMWAVEFORMAT (tag, channels, rate, avg, align, bits).
inline constexpr std::size_t kMinFmtSize = 16;

// Returns true when bytes hold a RIFF/WAVE file with a fmt chunk of at least
// kMinFmtSize bytes and a data chunk. Chunks may appear in any order and other
// chunks (LIST, fact, ...) are skipped; odd-sized chunks are padded to an even
// size as RIFF requires. A truncated fmt chunk is rejected; a truncated data
// chunk is clamped to the bytes present (mmioRead behaved the same way).
inline bool ParseRiffWave(const std::uint8_t* bytes, std::size_t size, WavChunks* out) {
    if (!bytes || !out || size < 12)
        return false;
    if (!detail::TagEquals(bytes, "RIFF") || !detail::TagEquals(bytes + 8, "WAVE"))
        return false;

    // The RIFF size may be wrong in files written by some tools; trust the
    // smaller of it and the real file size.
    std::size_t riffEnd = static_cast<std::size_t>(detail::ReadU32LE(bytes + 4));
    riffEnd = (riffEnd > size - 8) ? size : riffEnd + 8;

    bool haveFmt = false;
    bool haveData = false;
    WavChunks found;
    std::size_t pos = 12;
    while (pos + 8 <= riffEnd && !(haveFmt && haveData)) {
        const std::uint8_t* header = bytes + pos;
        const std::size_t chunkSize = detail::ReadU32LE(header + 4);
        const std::size_t body = pos + 8;
        const std::size_t available = riffEnd - body;

        if (!haveFmt && detail::TagEquals(header, "fmt ")) {
            if (chunkSize < kMinFmtSize || chunkSize > available)
                return false;
            found.fmtOffset = body;
            found.fmtSize = chunkSize;
            haveFmt = true;
        } else if (!haveData && detail::TagEquals(header, "data")) {
            found.dataOffset = body;
            found.dataSize = chunkSize > available ? available : chunkSize;
            haveData = true;
        }

        if (chunkSize > available)
            break;  // nothing can follow a chunk that runs past the end
        pos = body + chunkSize + (chunkSize & 1u);
    }

    if (!haveFmt || !haveData)
        return false;
    *out = found;
    return true;
}

// How many fmt-chunk bytes to hand XAudio2 as the voice format.
struct FormatCopy {
    std::size_t bytes = 0;    // bytes to copy from the start of the fmt body
    bool zeroCbSize = false;  // write cbSize = 0 after copying (PCM / float)
};

inline constexpr std::uint16_t kFormatPcm = 0x0001;
inline constexpr std::uint16_t kFormatIeeeFloat = 0x0003;
inline constexpr std::size_t kWaveFormatExSize = 18;  // sizeof(WAVEFORMATEX)

// Decides the format bytes CreateSourceVoice may read. XAudio2 reads
// sizeof(WAVEFORMATEX) + cbSize bytes, so the copy must hold all of them:
//  - PCM / IEEE float (and 16-17 byte fmt chunks): the first 16 bytes, cbSize 0
//    (cbSize is meaningless for these tags; some writers leave junk in it).
//  - Anything else (WAVE_FORMAT_EXTENSIBLE, ADPCM, ...): 18 + cbSize bytes,
//    which must fit both the fmt chunk and capacity. Otherwise returns false.
inline bool PlanFormatCopy(const std::uint8_t* fmt, std::size_t fmtSize, std::size_t capacity,
                           FormatCopy* out) {
    if (!fmt || !out || fmtSize < kMinFmtSize || capacity < kMinFmtSize)
        return false;
    const std::uint16_t tag = static_cast<std::uint16_t>(fmt[0] | (fmt[1] << 8));
    if (fmtSize < kWaveFormatExSize || tag == kFormatPcm || tag == kFormatIeeeFloat) {
        out->bytes = kMinFmtSize;
        out->zeroCbSize = true;
        return true;
    }
    const std::size_t cbSize = static_cast<std::size_t>(fmt[16] | (fmt[17] << 8));
    const std::size_t needed = kWaveFormatExSize + cbSize;
    if (needed > fmtSize || needed > capacity)
        return false;
    out->bytes = needed;
    out->zeroCbSize = false;
    return true;
}

}  // namespace wav_parse
