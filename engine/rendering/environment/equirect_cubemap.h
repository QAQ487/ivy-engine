// equirect_cubemap.h
// Portable equirectangular (lat-long) panorama to legacy DDS cubemap.
// No DirectX, WIC, or Windows headers. The editor/runtime loader lives in panorama_sky.cpp.

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace panorama {

struct Limits {
    int maxWidth = 8192;
    int maxHeight = 4096;
    int maxFace = 2048;
};

enum class SizeCode {
    Ok = 0,
    NonPositive,
    BadAspect,
    Oversized,
    FaceNotPowerOfTwo,
};

struct SizeCheck {
    SizeCode code = SizeCode::Ok;
    int faceSize = 0;
    std::string error;
};

// Face size is height/2. Aspect must be 2:1 and the face size must be a power of two.
SizeCheck ValidateEquirectSize(int width, int height, const Limits& limits = {});

struct Vec3 {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

// DirectX / texassemble face order: +X, -X, +Y, -Y, +Z, -Z.
// u,v are 0..1 across the face. Matches tools/bake_ibl.py face_uv_to_dir.
Vec3 CubeFaceDirection(int face, float u, float v);
void DirectionToFaceUv(float x, float y, float z, int* face, float* u, float* v);

// Debevec lat-long: +Z is the image center, +Y is the top row, u increases toward +X.
void EquirectUvFromDirection(float x, float y, float z, float* u, float* v);

struct RgbaImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

struct CubeBgra {
    int size = 0;
    // Face-major BGRA8: face 0..5, each size*size pixels.
    std::vector<std::uint8_t> faces;
};

bool ConvertEquirectToCube(const RgbaImage& image, int faceSize, CubeBgra* cube, std::string* error);

struct CubeLevel {
    int size = 0;
    std::vector<std::uint8_t> faces;
};

// Legacy (non-DX10) cubemap. Each face stores its full mip chain before the next face.
// Masks are BGRA8, the same layout tools/bake_ibl.py writes and CubemapLoad accepts.
bool WriteLegacyCubemapDds(const CubeLevel* levels, int levelCount, std::vector<std::uint8_t>* dds, std::string* error);

// Matches tools/bake_ibl.py: source is resampled to 256, irradiance is 32,
// prefilter is 128 with 5 mips (roughness = mip / 4), 64 samples.
struct IblConfig {
    int sourceSize = 256;
    int irradianceSize = 32;
    int prefilterSize = 128;
    int prefilterMips = 5;
    int samples = 64;
};

IblConfig ProductionIblConfig();

struct FloatCube {
    int size = 0;
    std::vector<float> faces;
};

void BakeIrradiance(const FloatCube& src, int samples, int dstSize, FloatCube* dst);
void BakePrefilter(const FloatCube& src, int samples, int baseSize, int mipCount, std::vector<FloatCube>* mips);

struct FileStamp {
    std::int64_t mtimeSec = 0;
    std::uint64_t size = 0;
    std::uint64_t contentHash = 0;
};

bool CacheIsFresh(const FileStamp& source, const FileStamp& recorded);
std::uint64_t Fnv1a64(const std::uint8_t* data, std::size_t size);
std::string FormatCacheStamp(const FileStamp& stamp);
bool ParseCacheStamp(const std::string& text, FileStamp* stamp);

// rom/skybox/panorama_test.png + "cube" -> rom/skybox/cache/skybox__panorama_test.dds
// kind is cube, irradiance, prefilter, or stamp. Empty when the path is not a safe rom PNG.
std::string PanoramaCacheRomPath(const std::string& pngRomPath, const char* kind);

struct CookPaths {
    std::filesystem::path cubemap;
    std::filesystem::path irradiance;
    std::filesystem::path prefilter;
    std::filesystem::path stamp;
};

struct DecodedImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

using DecodeImageFn = bool (*)(const std::uint8_t* bytes, std::size_t size, DecodedImage* image, std::string* error);

struct CookResult {
    bool ok = false;
    bool rebaked = false;
    bool iblReady = false;
    std::string error;
};

// Reads pngFile, skips work when the stamp matches mtime, size, and content hash.
// samples <= 0 writes the display cubemap and reports IBL as not matching the sky.
CookResult CookPanoramaFiles(
    const std::filesystem::path& pngFile,
    const CookPaths& outputs,
    DecodeImageFn decode,
    const Limits& limits,
    const IblConfig& ibl);

}  // namespace panorama
