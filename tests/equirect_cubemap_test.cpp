// Equirectangular panorama -> cubemap math, DDS layout, and cache invalidation.
// Build from the repo root:
//   g++ -std=c++20 -Wall -Wextra -I. -o /tmp/equirect_cubemap_test tests/equirect_cubemap_test.cpp engine/rendering/environment/equirect_cubemap.cpp

#include "engine/rendering/environment/equirect_cubemap.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int g_failed = 0;
static int g_decodeCalls = 0;

static void Expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failed;
    }
}

static std::uint32_t ReadU32(const std::vector<std::uint8_t>& d, std::size_t offset) {
    std::uint32_t v = 0;
    std::memcpy(&v, d.data() + offset, 4);
    return v;
}

static panorama::RgbaImage MakePattern(int width, int height) {
    panorama::RgbaImage image;
    image.width = width;
    image.height = height;
    image.rgba.resize(static_cast<std::size_t>(width * height * 4));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t i = static_cast<std::size_t>((y * width + x) * 4);
            image.rgba[i + 0] = static_cast<std::uint8_t>(x + 8 * y + 3);
            image.rgba[i + 1] = static_cast<std::uint8_t>((x * 3 + y * 5) & 255);
            image.rgba[i + 2] = static_cast<std::uint8_t>(11 + x);
            image.rgba[i + 3] = 255;
        }
    }
    return image;
}

static void ExpectPixel(const panorama::CubeBgra& cube, int face, int x, int y,
                        int r, int g, int b, const char* name) {
    const int size = cube.size;
    const std::size_t i = (static_cast<std::size_t>(face * size + y) * size + x) * 4;
    if (i + 3 >= cube.faces.size()) {
        std::fprintf(stderr, "FAIL: %s pixel out of range\n", name);
        ++g_failed;
        return;
    }
    const int br = cube.faces[i + 2];
    const int bg = cube.faces[i + 1];
    const int bb = cube.faces[i + 0];
    const int ba = cube.faces[i + 3];
    if (br != r || bg != g || bb != b || ba != 255) {
        std::fprintf(stderr, "FAIL: %s BGRA got %d,%d,%d,%d expected %d,%d,%d,255\n",
            name, bb, bg, br, ba, b, g, r);
        ++g_failed;
    }
}

static void TestFaceCenters() {
    const panorama::Vec3 faces[6] = {
        { 1.f, 0.f, 0.f },
        { -1.f, 0.f, 0.f },
        { 0.f, 1.f, 0.f },
        { 0.f, -1.f, 0.f },
        { 0.f, 0.f, 1.f },
        { 0.f, 0.f, -1.f },
    };
    for (int face = 0; face < 6; ++face) {
        const panorama::Vec3 d = panorama::CubeFaceDirection(face, 0.5f, 0.5f);
        const float dx = d.x - faces[face].x;
        const float dy = d.y - faces[face].y;
        const float dz = d.z - faces[face].z;
        if (dx * dx + dy * dy + dz * dz > 1e-6f) {
            std::fprintf(stderr, "FAIL: face %d center (%g %g %g)\n", face, d.x, d.y, d.z);
            ++g_failed;
        }
        int back = -1;
        float u = 0.f;
        float v = 0.f;
        panorama::DirectionToFaceUv(d.x, d.y, d.z, &back, &u, &v);
        Expect(back == face, "face center maps back to the same face");
        Expect(std::fabs(u - 0.5f) < 1e-4f && std::fabs(v - 0.5f) < 1e-4f,
            "face center uv is the middle of the face");
    }

    float u = 0.f;
    float v = 0.f;
    panorama::EquirectUvFromDirection(0.f, 0.f, 1.f, &u, &v);
    Expect(std::fabs(u - 0.5f) < 1e-4f && std::fabs(v - 0.5f) < 1e-4f,
        "+Z is the center of the equirectangular image");
    panorama::EquirectUvFromDirection(0.f, 1.f, 0.f, &u, &v);
    Expect(std::fabs(v) < 1e-4f, "+Y is the top edge of the equirectangular image");
    panorama::EquirectUvFromDirection(0.f, -1.f, 0.f, &u, &v);
    Expect(std::fabs(v - 1.f) < 1e-4f, "-Y is the bottom edge of the equirectangular image");

    // 180 deg FOV overload must match the legacy 2:1 mapping.
    float u180 = 0.f;
    float v180 = 0.f;
    panorama::EquirectUvFromDirection(0.f, 0.f, 1.f, 180.f, &u180, &v180);
    Expect(std::fabs(u180 - 0.5f) < 1e-4f && std::fabs(v180 - 0.5f) < 1e-4f,
        "180 deg FOV keeps +Z at the image center");
}

static void TestRoundTrip() {
    const float samples[] = { 0.2f, 0.5f, 0.8f };
    for (int face = 0; face < 6; ++face) {
        for (float u : samples) {
            for (float v : samples) {
                const panorama::Vec3 d = panorama::CubeFaceDirection(face, u, v);
                int back = -1;
                float bu = 0.f;
                float bv = 0.f;
                panorama::DirectionToFaceUv(d.x, d.y, d.z, &back, &bu, &bv);
                if (back != face || std::fabs(bu - u) > 1e-4f || std::fabs(bv - v) > 1e-4f) {
                    std::fprintf(stderr, "FAIL: round trip face %d uv %g %g -> %d %g %g\n",
                        face, u, v, back, bu, bv);
                    ++g_failed;
                }
            }
        }
    }
}

static void TestConvertPixels() {
    const panorama::RgbaImage image = MakePattern(8, 4);
    panorama::CubeBgra cube;
    std::string error;
    Expect(panorama::ConvertEquirectToCube(image, 2, 180.0, &cube, &error), "convert 8x4");
    Expect(error.empty(), "convert error is empty");
    Expect(cube.size == 2, "face size 2");
    Expect(cube.faces.size() == 6u * 2u * 2u * 4u, "six BGRA faces");

    struct Sample {
        int face, x, y, r, g, b;
    };
    // Independent double-precision trace of the Debevec lat-long map.
    const Sample expected[] = {
        { 0, 0, 0, 16, 20, 16 }, { 0, 1, 1, 25, 28, 17 },
        { 1, 0, 0, 12, 8, 12 }, { 1, 1, 1, 21, 16, 13 },
        { 2, 0, 0, 6, 3, 12 }, { 2, 1, 1, 10, 15, 16 },
        { 3, 0, 0, 27, 21, 14 }, { 3, 1, 1, 31, 33, 18 },
        { 4, 0, 0, 14, 14, 14 }, { 4, 1, 1, 23, 22, 15 },
        { 5, 0, 0, 18, 26, 18 }, { 5, 1, 1, 19, 10, 11 },
    };
    for (const Sample& s : expected) {
        char name[64];
        std::snprintf(name, sizeof(name), "face %d pixel %d,%d", s.face, s.x, s.y);
        ExpectPixel(cube, s.face, s.x, s.y, s.r, s.g, s.b, name);
    }
}

static void TestCoverageMath() {
    Expect(std::fabs(panorama::DefaultVerticalFovDeg(8, 4) - 180.0) < 1e-9,
        "2:1 default coverage is 180 deg");
    Expect(std::fabs(panorama::DefaultVerticalFovDeg(1024, 354) - (360.0 * 354.0 / 1024.0)) < 1e-9,
        "1024x354 default coverage is 360*h/w");
    Expect(panorama::DefaultVerticalFovDeg(8, 5) == 0.0, "height > width/2 has no default FOV");
    Expect(panorama::DefaultVerticalFovDeg(0, 4) == 0.0, "degenerate width has no default FOV");

    Expect(std::fabs(panorama::ResolveVerticalFovDeg(1024, 354, 0.0) - (360.0 * 354.0 / 1024.0)) < 1e-9,
        "override 0 uses the image default");
    Expect(std::fabs(panorama::ResolveVerticalFovDeg(1024, 354, 90.0) - 90.0) < 1e-9,
        "positive override is kept");
    Expect(std::fabs(panorama::ResolveVerticalFovDeg(1024, 354, 200.0) - 180.0) < 1e-9,
        "override above 180 clamps to 180");
    Expect(panorama::ResolveVerticalFovDeg(1024, 354, -5.0) > 0.0,
        "negative override falls back to the image default");

    float u = 0.f;
    float v = 0.f;
    // 90 deg band: horizon stays at v=0.5; +/-45 deg elevation hits the image edges.
    const float elev45 = std::sin(static_cast<float>(3.14159265358979323846 / 4.0));
    const float horiz45 = std::cos(static_cast<float>(3.14159265358979323846 / 4.0));
    panorama::EquirectUvFromDirection(0.f, elev45, horiz45, 90.f, &u, &v);
    Expect(std::fabs(u - 0.5f) < 1e-4f && std::fabs(v) < 1e-3f,
        "90 deg FOV maps +45 elev to the top row");
    panorama::EquirectUvFromDirection(0.f, -elev45, horiz45, 90.f, &u, &v);
    Expect(std::fabs(u - 0.5f) < 1e-4f && std::fabs(v - 1.f) < 1e-3f,
        "90 deg FOV maps -45 elev to the bottom row");

    // Outside the covered band clamps to the nearest edge (no wrap).
    panorama::EquirectUvFromDirection(0.f, 1.f, 0.f, 90.f, &u, &v);
    Expect(std::fabs(v) < 1e-6f, "+Y clamps to the top row under 90 deg FOV");
    panorama::EquirectUvFromDirection(0.f, -1.f, 0.f, 90.f, &u, &v);
    Expect(std::fabs(v - 1.f) < 1e-6f, "-Y clamps to the bottom row under 90 deg FOV");

    // Outside the image band, convert clamps to the edge rows (no pole stretch).
    panorama::RgbaImage band;
    band.width = 8;
    band.height = 3;
    band.rgba.assign(static_cast<std::size_t>(8 * 3 * 4), 0);
    for (int x = 0; x < 8; ++x) {
        const std::size_t top = static_cast<std::size_t>(x * 4);
        const std::size_t bot = static_cast<std::size_t>((2 * 8 + x) * 4);
        band.rgba[top + 0] = 255;
        band.rgba[top + 1] = 0;
        band.rgba[top + 2] = 0;
        band.rgba[top + 3] = 255;
        band.rgba[bot + 0] = 0;
        band.rgba[bot + 1] = 0;
        band.rgba[bot + 2] = 255;
        band.rgba[bot + 3] = 255;
    }
    panorama::CubeBgra cube;
    std::string error;
    Expect(panorama::ConvertEquirectToCube(band, 1, 90.0, &cube, &error), "convert 8x3 partial");
    ExpectPixel(cube, 2, 0, 0, 255, 0, 0, "+Y face clamps to the top row under partial FOV");
    ExpectPixel(cube, 3, 0, 0, 0, 0, 255, "-Y face clamps to the bottom row under partial FOV");
}

static void TestValidation() {
    const panorama::Limits limits;
    const panorama::SizeCheck ok = panorama::ValidateEquirectSize(8, 4, limits);
    Expect(ok.code == panorama::SizeCode::Ok && ok.faceSize == 2, "8x4 face is 2");

    const panorama::SizeCheck partial = panorama::ValidateEquirectSize(8, 3, limits);
    Expect(partial.code == panorama::SizeCode::Ok && partial.faceSize == 2,
        "8x3 partial coverage is accepted with face width/4");

    const panorama::SizeCheck church = panorama::ValidateEquirectSize(1024, 354, limits);
    Expect(church.code == panorama::SizeCode::Ok && church.faceSize == 256,
        "1024x354 church panorama uses face 256");

    const panorama::SizeCheck aspect = panorama::ValidateEquirectSize(8, 5, limits);
    Expect(aspect.code == panorama::SizeCode::BadAspect, "height > width/2 is invalid");
    Expect(aspect.error.find("180") != std::string::npos, "invalid aspect mentions 180 deg");

    const panorama::SizeCheck empty = panorama::ValidateEquirectSize(0, 0, limits);
    Expect(empty.code == panorama::SizeCode::NonPositive, "empty image");

    const panorama::SizeCheck odd = panorama::ValidateEquirectSize(6, 3, limits);
    Expect(odd.code == panorama::SizeCode::FaceNotPowerOfTwo, "odd height is not a face size");
    Expect(odd.error.find("power of two") != std::string::npos, "odd height mentions power of two");

    const panorama::SizeCheck pot = panorama::ValidateEquirectSize(12, 6, limits);
    Expect(pot.code == panorama::SizeCode::FaceNotPowerOfTwo && pot.faceSize == 3,
        "face 3 is not a power of two");

    const panorama::SizeCheck huge = panorama::ValidateEquirectSize(16384, 8192, limits);
    Expect(huge.code == panorama::SizeCode::Oversized, "16384x8192 is oversized");
    Expect(huge.error.find("oversized") != std::string::npos, "oversized error text");

    const panorama::SizeCheck limit = panorama::ValidateEquirectSize(8192, 4096, limits);
    Expect(limit.code == panorama::SizeCode::Ok && limit.faceSize == 2048, "8192x4096 is the maximum");
}

static void TestDdsLayout() {
    panorama::CubeLevel levels[2];
    levels[0].size = 2;
    levels[0].faces.assign(6 * 2 * 2 * 4, 0);
    levels[1].size = 1;
    levels[1].faces.assign(6 * 1 * 1 * 4, 0);
    levels[0].faces[0] = 0x11;
    levels[0].faces[1] = 0x22;
    levels[0].faces[2] = 0x33;
    levels[0].faces[3] = 0xFF;
    levels[1].faces[0] = 0xAB;
    levels[1].faces[1] = 0xCD;
    levels[1].faces[2] = 0xEF;
    levels[1].faces[3] = 0xFF;

    std::vector<std::uint8_t> dds;
    std::string error;
    Expect(panorama::WriteLegacyCubemapDds(levels, 2, &dds, &error), "write mip dds");
    Expect(dds.size() == 128u + (16u + 4u) * 6u, "header plus per-face mip chain");
    Expect(std::memcmp(dds.data(), "DDS ", 4) == 0, "DDS magic");
    Expect(ReadU32(dds, 4) == 124u, "header size");
    Expect(ReadU32(dds, 8) == 0x2100fu, "mipmapped cubemap flags");
    Expect(ReadU32(dds, 12) == 2u && ReadU32(dds, 16) == 2u, "base size");
    Expect(ReadU32(dds, 20) == 8u, "pitch");
    Expect(ReadU32(dds, 24) == 0u, "depth unused");
    Expect(ReadU32(dds, 28) == 2u, "mip count");
    Expect(ReadU32(dds, 76) == 32u, "pixel format size");
    Expect(ReadU32(dds, 80) == 0x41u, "RGB + alpha pixel format");
    Expect(ReadU32(dds, 88) == 32u, "32-bit pixels");
    Expect(ReadU32(dds, 92) == 0x00FF0000u, "red mask");
    Expect(ReadU32(dds, 96) == 0x0000FF00u, "green mask");
    Expect(ReadU32(dds, 100) == 0x000000FFu, "blue mask");
    Expect(ReadU32(dds, 104) == 0xFF000000u, "alpha mask");
    Expect(ReadU32(dds, 108) == 0x401008u, "texture complex mip caps");
    Expect(ReadU32(dds, 112) == 0xFE00u, "all six cubemap faces");
    Expect(dds[128] == 0x11 && dds[129] == 0x22 && dds[130] == 0x33, "face 0 mip 0 BGRA");
    Expect(dds[128 + 16] == 0xAB && dds[128 + 17] == 0xCD && dds[128 + 18] == 0xEF,
        "mip chain stays on the same face before the next face");

    panorama::CubeLevel one = levels[0];
    one.faces[0] = 9;
    std::vector<std::uint8_t> single;
    Expect(panorama::WriteLegacyCubemapDds(&one, 1, &single, &error), "write single mip");
    Expect(ReadU32(single, 8) == 0x100fu, "single mip omits the mipmap-count flag");
    Expect(ReadU32(single, 108) == 0x1008u, "single mip caps");
    Expect(ReadU32(single, 28) == 1u, "single mip count field");
}

static panorama::FloatCube SolidCube(int size, float r, float g, float b) {
    panorama::FloatCube cube;
    cube.size = size;
    cube.faces.assign(static_cast<std::size_t>(6 * size * size * 3), 0.f);
    for (std::size_t i = 0; i < cube.faces.size(); i += 3) {
        cube.faces[i] = r;
        cube.faces[i + 1] = g;
        cube.faces[i + 2] = b;
    }
    return cube;
}

static void TestIbl() {
    const panorama::IblConfig production = panorama::ProductionIblConfig();
    Expect(production.sourceSize == 256, "IBL source matches bake_ibl.py");
    Expect(production.irradianceSize == 32, "irradiance face is 32");
    Expect(production.prefilterSize == 128, "prefilter base is 128");
    Expect(production.prefilterMips == 5, "prefilter has 5 mips");
    Expect(production.samples == 64, "sample count matches bake_ibl.py");

    const panorama::FloatCube white = SolidCube(4, 1.f, 1.f, 1.f);
    panorama::FloatCube irradiance;
    panorama::BakeIrradiance(white, 8, 2, &irradiance);
    Expect(irradiance.size == 2, "irradiance size");
    for (float c : irradiance.faces) {
        if (std::fabs(c - 1.f) > 1e-3f) {
            std::fprintf(stderr, "FAIL: white irradiance sample %g\n", c);
            ++g_failed;
            break;
        }
    }

    panorama::FloatCube colored = SolidCube(4, 0.f, 0.f, 0.f);
    for (int i = 0; i < 4 * 4; ++i) {
        colored.faces[static_cast<std::size_t>(i * 3)] = 1.f;
    }
    std::vector<panorama::FloatCube> mips;
    panorama::BakePrefilter(colored, 4, 4, 3, &mips);
    Expect(mips.size() == 3u, "three prefilter mips");
    Expect(mips[0].size == 4 && mips[1].size == 2 && mips[2].size == 1, "mip sizes halve");
    const std::size_t center = (static_cast<std::size_t>(0 * 4 + 2) * 4 + 2) * 3;
    Expect(std::fabs(mips[0].faces[center] - 1.f) < 1e-3f, "roughness 0 keeps +X center");
    Expect(mips[0].faces[center + 1] < 1e-3f, "roughness 0 does not bleed green into +X");
}

static void TestStamp() {
    Expect(panorama::Fnv1a64(nullptr, 0) == 14695981039346656037ull, "FNV offset basis");
    const std::uint8_t abc[] = { 'a', 'b', 'c' };
    Expect(panorama::Fnv1a64(abc, 3) == 0xe71fa2190541574bull, "FNV-1a 64 of abc");

    panorama::FileStamp stamp;
    stamp.mtimeSec = 1700000000;
    stamp.size = 12;
    stamp.contentHash = 0xe71fa2190541574bull;
    stamp.verticalFovDeg = 0;
    panorama::FileStamp copy;
    Expect(panorama::ParseCacheStamp(panorama::FormatCacheStamp(stamp), &copy), "parse stamp");
    Expect(panorama::CacheIsFresh(stamp, copy), "formatted stamp matches");
    Expect(copy.verticalFovDeg == 0.0, "auto FOV round-trips as 0");

    panorama::FileStamp withFov = stamp;
    withFov.verticalFovDeg = 90.0;
    panorama::FileStamp fovCopy;
    Expect(panorama::ParseCacheStamp(panorama::FormatCacheStamp(withFov), &fovCopy), "parse FOV stamp");
    Expect(panorama::CacheIsFresh(withFov, fovCopy), "FOV stamp matches");
    Expect(std::fabs(fovCopy.verticalFovDeg - 90.0) < 1e-9, "explicit FOV round-trips");
    Expect(!panorama::CacheIsFresh(stamp, withFov), "FOV change invalidates the cache");

    panorama::FileStamp changed = stamp;
    changed.mtimeSec += 1;
    Expect(!panorama::CacheIsFresh(stamp, changed), "mtime change invalidates the cache");
    changed = stamp;
    changed.size = 11;
    Expect(!panorama::CacheIsFresh(stamp, changed), "size change invalidates the cache");
    changed = stamp;
    changed.contentHash ^= 1ull;
    Expect(!panorama::CacheIsFresh(stamp, changed), "content hash change invalidates the cache");
    changed = stamp;
    changed.size = 0;
    Expect(!panorama::CacheIsFresh(changed, changed), "empty source is not a fresh cache");

    Expect(panorama::PanoramaCacheRomPath("rom/skybox/panorama_test.png", "cube") ==
        "rom/skybox/cache/skybox__panorama_test.dds", "cube cache path");
    Expect(panorama::PanoramaCacheRomPath("rom\\skybox\\panorama_test.png", "irradiance") ==
        "rom/skybox/cache/skybox__panorama_test.irradiance.dds", "backslash cache path");
    Expect(panorama::PanoramaCacheRomPath("rom/skybox/panorama_test.png", "prefilter") ==
        "rom/skybox/cache/skybox__panorama_test.prefilter.dds", "prefilter cache path");
    Expect(panorama::PanoramaCacheRomPath("rom/skybox/panorama_test.png", "stamp") ==
        "rom/skybox/cache/skybox__panorama_test.stamp", "stamp cache path");
    Expect(panorama::PanoramaCacheRomPath("rom/../secret.png", "cube").empty(), "reject traversal");
    Expect(panorama::PanoramaCacheRomPath("rom/skybox/MySkybox.dds", "cube").empty(),
        "DDS sources are not panorama caches");
}

static bool TestDecode(const std::uint8_t* bytes, std::size_t n, panorama::DecodedImage* image, std::string* error) {
    ++g_decodeCalls;
    if (!bytes || n < 1) {
        if (error) *error = "decode failed";
        return false;
    }
    if (bytes[0] == 0xFF) {
        if (error) *error = "decode failed";
        return false;
    }
    if (bytes[0] == 'O') {
        image->width = 16384;
        image->height = 8192;
        image->rgba.clear();
        return true;
    }
    if (bytes[0] == 'A') {
        image->width = 8;
        image->height = 5;
        image->rgba.clear();
        return true;
    }
    if (bytes[0] == 'N') {
        image->width = 8;
        image->height = 3;
        image->rgba.assign(static_cast<std::size_t>(8 * 3 * 4), 180);
        return true;
    }
    if (bytes[0] == 'P') {
        image->width = 12;
        image->height = 6;
        image->rgba.clear();
        return true;
    }
    if (n < 4) {
        if (error) *error = "decode failed";
        return false;
    }
    const int w = bytes[0] | (bytes[1] << 8);
    const int h = bytes[2] | (bytes[3] << 8);
    const std::size_t need = 4u + static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u;
    if (w <= 0 || h <= 0 || n < need) {
        if (error) *error = "decode failed";
        return false;
    }
    image->width = w;
    image->height = h;
    image->rgba.assign(bytes + 4, bytes + need);
    return true;
}

static std::vector<std::uint8_t> TinyPngBytes() {
    std::vector<std::uint8_t> bytes = { 4, 0, 2, 0 };
    bytes.resize(4 + 4 * 2 * 4, 200);
    return bytes;
}

static void WriteBytes(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

static panorama::IblConfig TinyIbl() {
    panorama::IblConfig config;
    config.sourceSize = 2;
    config.irradianceSize = 2;
    config.prefilterSize = 2;
    config.prefilterMips = 2;
    config.samples = 4;
    return config;
}

static void TestCookCache(const fs::path& dir) {
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    const fs::path png = dir / "in.bin";
    panorama::CookPaths out;
    out.cubemap = dir / "cube.dds";
    out.irradiance = dir / "irr.dds";
    out.prefilter = dir / "pre.dds";
    out.stamp = dir / "cube.stamp";
    const panorama::Limits limits;
    const panorama::IblConfig ibl = TinyIbl();

    g_decodeCalls = 0;
    const panorama::CookResult missing = panorama::CookPanoramaFiles(
        dir / "missing.bin", out, TestDecode, limits, ibl);
    Expect(!missing.ok, "missing panorama fails");
    Expect(missing.error.find("not found") != std::string::npos, "missing file error");
    Expect(g_decodeCalls == 0, "missing file does not decode");

    WriteBytes(png, { 'A' });
    const panorama::CookResult aspect = panorama::CookPanoramaFiles(png, out, TestDecode, limits, ibl);
    Expect(!aspect.ok && aspect.error.find("180") != std::string::npos, "cook rejects coverage over 180 deg");

    WriteBytes(png, { 'P' });
    const panorama::CookResult face = panorama::CookPanoramaFiles(png, out, TestDecode, limits, ibl);
    Expect(!face.ok && face.error.find("power of two") != std::string::npos, "cook rejects non power-of-two face");

    WriteBytes(png, { 'O' });
    const panorama::CookResult huge = panorama::CookPanoramaFiles(png, out, TestDecode, limits, ibl);
    Expect(!huge.ok && huge.error.find("oversized") != std::string::npos, "cook rejects oversized panorama");

    WriteBytes(png, { 0xFF });
    const panorama::CookResult bad = panorama::CookPanoramaFiles(png, out, TestDecode, limits, ibl);
    Expect(!bad.ok && bad.error.find("decode failed") != std::string::npos, "decode errors surface");

    WriteBytes(png, { 'N' });
    const panorama::CookResult partial = panorama::CookPanoramaFiles(png, out, TestDecode, limits, ibl);
    Expect(partial.ok && partial.rebaked, "non-2:1 panorama cooks");
    Expect(partial.warning.find("360") != std::string::npos ||
           partial.warning.find("coverage") != std::string::npos,
        "non-2:1 cook warns about assumed coverage");

    WriteBytes(png, TinyPngBytes());
    panorama::IblConfig skip = ibl;
    skip.samples = 0;
    const panorama::CookResult skipped = panorama::CookPanoramaFiles(png, out, TestDecode, limits, skip);
    Expect(skipped.ok, "cubemap still cooks when IBL is skipped");
    Expect(!skipped.iblReady, "skipped IBL is not ready");
    Expect(skipped.error.find("IBL not matching sky") != std::string::npos, "skipped IBL is explicit");
    Expect(fs::is_regular_file(out.cubemap), "display cubemap was written");
    Expect(!fs::is_regular_file(out.stamp), "stamp is withheld until IBL matches");
    Expect(!fs::is_regular_file(out.irradiance) && !fs::is_regular_file(out.prefilter),
        "stale IBL files are not left behind");

    g_decodeCalls = 0;
    const panorama::CookResult first = panorama::CookPanoramaFiles(png, out, TestDecode, limits, ibl);
    Expect(first.ok && first.rebaked && first.iblReady, "first cook bakes cubemap and IBL");
    Expect(g_decodeCalls == 1, "first cook decodes once");
    Expect(fs::is_regular_file(out.irradiance) && fs::is_regular_file(out.prefilter), "IBL files exist");
    const auto cubeTime = fs::last_write_time(out.cubemap);
    const auto irrTime = fs::last_write_time(out.irradiance);

    const panorama::CookResult second = panorama::CookPanoramaFiles(png, out, TestDecode, limits, ibl);
    Expect(second.ok && !second.rebaked && second.iblReady, "fresh stamp skips the bake");
    Expect(g_decodeCalls == 1, "fresh cache does not decode again");
    Expect(fs::last_write_time(out.cubemap) == cubeTime, "fresh cache does not rewrite the cubemap");
    Expect(fs::last_write_time(out.irradiance) == irrTime, "fresh cache does not rewrite irradiance");

    g_decodeCalls = 1;
    const panorama::CookResult fovChange = panorama::CookPanoramaFiles(
        png, out, TestDecode, limits, ibl, 90.0);
    Expect(fovChange.ok && fovChange.rebaked, "vertical FOV override rebakes");
    Expect(g_decodeCalls == 2, "FOV change decodes again");

    const panorama::CookResult fovFresh = panorama::CookPanoramaFiles(
        png, out, TestDecode, limits, ibl, 90.0);
    Expect(fovFresh.ok && !fovFresh.rebaked, "matching FOV keeps the cache fresh");

    const panorama::CookResult fovAuto = panorama::CookPanoramaFiles(
        png, out, TestDecode, limits, ibl, 0.0);
    Expect(fovAuto.ok && fovAuto.rebaked, "switching FOV back to auto rebakes");

    const std::vector<std::uint8_t> same = TinyPngBytes();
    WriteBytes(png, same);
    fs::last_write_time(png, fs::last_write_time(png) + std::chrono::seconds(10));
    const panorama::CookResult touched = panorama::CookPanoramaFiles(png, out, TestDecode, limits, ibl);
    Expect(touched.rebaked, "mtime change rebakes even when the bytes match");

    std::vector<std::uint8_t> edited = TinyPngBytes();
    edited.back() ^= 0x5A;
    WriteBytes(png, edited);
    const panorama::CookResult editedCook = panorama::CookPanoramaFiles(png, out, TestDecode, limits, ibl);
    Expect(editedCook.rebaked && editedCook.iblReady, "content change rebakes");

    std::ifstream pre(out.prefilter, std::ios::binary);
    std::vector<std::uint8_t> header(128);
    pre.read(reinterpret_cast<char*>(header.data()), 128);
    Expect(ReadU32(header, 16) == 2u && ReadU32(header, 28) == 2u, "prefilter DDS uses the requested mip chain");
    Expect(ReadU32(header, 112) == 0xFE00u, "prefilter DDS is a cubemap");

    fs::remove_all(dir, ec);
}

int main() {
    TestFaceCenters();
    TestRoundTrip();
    TestConvertPixels();
    TestCoverageMath();
    TestValidation();
    TestDdsLayout();
    TestIbl();
    TestStamp();
    TestCookCache(fs::temp_directory_path() / "ivy_equirect_cubemap_test");
    if (g_failed) {
        std::fprintf(stderr, "%d assertion(s) failed\n", g_failed);
        return 1;
    }
    std::printf("EQUIRECT_CUBEMAP_TEST PASS\n");
    return 0;
}
