// wav_parse_test.cpp
// Linux-runnable unit tests for engine/audio/wav_parse.h.
//
// Build from the repo root:
//   g++ -std=c++17 -Wall -Wextra -I. -o /tmp/wav_parse_test tests/wav_parse_test.cpp
//   /tmp/wav_parse_test

#include "engine/audio/wav_parse.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

static int g_failed = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

using Bytes = std::vector<std::uint8_t>;

static void PutTag(Bytes& b, const char* tag) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>(tag[i]));
}

static void PutU32(Bytes& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu));
}

static void PutChunk(Bytes& b, const char* tag, const Bytes& body, bool pad = true) {
    PutTag(b, tag);
    PutU32(b, static_cast<std::uint32_t>(body.size()));
    b.insert(b.end(), body.begin(), body.end());
    if (pad && (body.size() & 1u)) b.push_back(0);
}

// Wraps chunks in a RIFF/WAVE header with a correct RIFF size.
static Bytes Riff(const Bytes& chunks) {
    Bytes b;
    PutTag(b, "RIFF");
    PutU32(b, static_cast<std::uint32_t>(4 + chunks.size()));
    PutTag(b, "WAVE");
    b.insert(b.end(), chunks.begin(), chunks.end());
    return b;
}

static Bytes Fmt(std::size_t size) {
    Bytes f(size, 0);
    f[0] = 1;  // PCM
    f[2] = 2;  // stereo
    return f;
}

static bool Parse(const Bytes& b, wav_parse::WavChunks* out) {
    return wav_parse::ParseRiffWave(b.data(), b.size(), out);
}

int main() {
    using wav_parse::WavChunks;

    // Plain PCM: fmt (16) then data.
    {
        Bytes c;
        PutChunk(c, "fmt ", Fmt(16));
        PutChunk(c, "data", Bytes{1, 2, 3, 4});
        const Bytes b = Riff(c);
        WavChunks w;
        Expect(Parse(b, &w), "plain PCM parses");
        Expect(w.fmtOffset == 20 && w.fmtSize == 16, "fmt offset/size");
        Expect(w.dataOffset == 44 && w.dataSize == 4, "data offset/size");
        Expect(b[w.dataOffset] == 1 && b[w.dataOffset + 3] == 4, "data bytes");
    }

    // WAVEFORMATEX-sized fmt (18) and a LIST chunk with odd size before data.
    {
        Bytes c;
        PutChunk(c, "fmt ", Fmt(18));
        PutChunk(c, "LIST", Bytes{9, 9, 9});  // odd -> one pad byte
        PutChunk(c, "data", Bytes{7, 8});
        const Bytes b = Riff(c);
        WavChunks w;
        Expect(Parse(b, &w), "fmt 18 + odd LIST parses");
        Expect(w.fmtSize == 18, "fmt 18 size");
        Expect(w.dataSize == 2 && b[w.dataOffset] == 7, "data after padded LIST");
    }

    // data before fmt is accepted.
    {
        Bytes c;
        PutChunk(c, "data", Bytes{5});
        PutChunk(c, "fmt ", Fmt(16));
        WavChunks w;
        const Bytes b = Riff(c);
        Expect(Parse(b, &w), "data before fmt parses");
        Expect(w.dataSize == 1 && b[w.dataOffset] == 5, "data before fmt bytes");
        Expect(w.fmtSize == 16, "fmt after data");
    }

    // Truncated data chunk is clamped to the bytes present.
    {
        Bytes c;
        PutChunk(c, "fmt ", Fmt(16));
        PutTag(c, "data");
        PutU32(c, 100);
        c.push_back(1);
        c.push_back(2);
        WavChunks w;
        Expect(Parse(Riff(c), &w), "truncated data parses");
        Expect(w.dataSize == 2, "truncated data clamped");
    }

    // RIFF size larger than the file is tolerated.
    {
        Bytes c;
        PutChunk(c, "fmt ", Fmt(16));
        PutChunk(c, "data", Bytes{1, 2});
        Bytes b = Riff(c);
        b[4] = 0xFF; b[5] = 0xFF; b[6] = 0xFF; b[7] = 0x7F;
        WavChunks w;
        Expect(Parse(b, &w), "oversized RIFF size parses");
        Expect(w.dataSize == 2, "oversized RIFF data size");
    }

    // Rejections.
    {
        WavChunks w;
        Expect(!wav_parse::ParseRiffWave(nullptr, 0, &w), "null bytes rejected");
        Expect(!Parse(Bytes{}, &w), "empty rejected");
        Bytes c;
        PutChunk(c, "fmt ", Fmt(16));
        PutChunk(c, "data", Bytes{1});
        Expect(!wav_parse::ParseRiffWave(Riff(c).data(), Riff(c).size(), nullptr), "null out rejected");

        Bytes notRiff = Riff(c);
        notRiff[0] = 'X';
        Expect(!Parse(notRiff, &w), "non-RIFF rejected");
        Bytes notWave = Riff(c);
        notWave[8] = 'A';
        Expect(!Parse(notWave, &w), "non-WAVE rejected");

        Bytes noData;
        PutChunk(noData, "fmt ", Fmt(16));
        Expect(!Parse(Riff(noData), &w), "missing data rejected");

        Bytes noFmt;
        PutChunk(noFmt, "data", Bytes{1, 2});
        Expect(!Parse(Riff(noFmt), &w), "missing fmt rejected");

        Bytes smallFmt;
        PutChunk(smallFmt, "fmt ", Fmt(14));
        PutChunk(smallFmt, "data", Bytes{1});
        Expect(!Parse(Riff(smallFmt), &w), "fmt smaller than PCMWAVEFORMAT rejected");

        Bytes cutFmt;
        PutTag(cutFmt, "fmt ");
        PutU32(cutFmt, 16);
        cutFmt.insert(cutFmt.end(), 8, 0);
        Expect(!Parse(Riff(cutFmt), &w), "truncated fmt rejected");

        Bytes hugeChunk;
        PutChunk(hugeChunk, "fmt ", Fmt(16));
        PutTag(hugeChunk, "JUNK");
        PutU32(hugeChunk, 0xFFFFFFFFu);
        Expect(!Parse(Riff(hugeChunk), &w), "huge junk chunk stops scan without data");
    }

    // PlanFormatCopy: how much of the fmt chunk becomes the voice format.
    {
        using wav_parse::FormatCopy;
        using wav_parse::PlanFormatCopy;
        auto fmt = [](std::uint16_t tag, std::size_t size, std::uint16_t cbSize) {
            Bytes f(size, 0);
            f[0] = static_cast<std::uint8_t>(tag & 0xFFu);
            f[1] = static_cast<std::uint8_t>(tag >> 8);
            if (size >= 18) {
                f[16] = static_cast<std::uint8_t>(cbSize & 0xFFu);
                f[17] = static_cast<std::uint8_t>(cbSize >> 8);
            }
            return f;
        };
        FormatCopy c;

        Bytes pcm16 = fmt(1, 16, 0);
        Expect(PlanFormatCopy(pcm16.data(), pcm16.size(), 64, &c) && c.bytes == 16 && c.zeroCbSize,
               "PCM 16-byte fmt -> 16 bytes, cbSize 0");
        Bytes pcmJunk = fmt(1, 18, 0xBEEF);
        Expect(PlanFormatCopy(pcmJunk.data(), pcmJunk.size(), 64, &c) && c.bytes == 16 && c.zeroCbSize,
               "PCM with junk cbSize -> cbSize ignored");
        Bytes flt = fmt(3, 18, 0);
        Expect(PlanFormatCopy(flt.data(), flt.size(), 64, &c) && c.bytes == 16 && c.zeroCbSize,
               "IEEE float -> 16 bytes, cbSize 0");

        Bytes ext = fmt(0xFFFE, 40, 22);
        Expect(PlanFormatCopy(ext.data(), ext.size(), 64, &c) && c.bytes == 40 && !c.zeroCbSize,
               "WAVE_FORMAT_EXTENSIBLE -> all 40 bytes");
        Bytes adpcm = fmt(2, 50, 32);
        Expect(PlanFormatCopy(adpcm.data(), adpcm.size(), 64, &c) && c.bytes == 50 && !c.zeroCbSize,
               "MS ADPCM (cbSize 32) -> 50 bytes");
        Bytes zeroCb = fmt(0x0161, 18, 0);
        Expect(PlanFormatCopy(zeroCb.data(), zeroCb.size(), 64, &c) && c.bytes == 18,
               "other tag with cbSize 0 -> 18 bytes");
        Bytes shortOther = fmt(2, 16, 0);
        Expect(PlanFormatCopy(shortOther.data(), shortOther.size(), 64, &c) && c.bytes == 16 && c.zeroCbSize,
               "16-byte fmt of any tag -> 16 bytes, cbSize 0");

        Bytes cbPastChunk = fmt(0xFFFE, 30, 22);
        Expect(!PlanFormatCopy(cbPastChunk.data(), cbPastChunk.size(), 64, &c),
               "cbSize past the fmt chunk rejected");
        Bytes cbPastCapacity = fmt(0xFFFE, 200, 100);
        Expect(!PlanFormatCopy(cbPastCapacity.data(), cbPastCapacity.size(), 64, &c),
               "cbSize past the storage rejected");
        Expect(!PlanFormatCopy(ext.data(), ext.size(), 39, &c), "capacity one short rejected");
        Expect(!PlanFormatCopy(nullptr, 40, 64, &c), "null fmt rejected");
        Expect(!PlanFormatCopy(ext.data(), ext.size(), 64, nullptr), "null out rejected");
        Expect(!PlanFormatCopy(ext.data(), 14, 64, &c), "fmt below 16 bytes rejected");
    }

    if (g_failed) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("wav_parse_test: all checks passed\n");
    return 0;
}
