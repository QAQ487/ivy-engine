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

// Face size is height/2 when the image is exactly 2:1; otherwise the largest power of
// two at most width/4. Width is full 360 deg. Height may be less than width/2 (partial
// vertical coverage). Height greater than width/2 (more than 180 deg) is rejected.
SizeCheck ValidateEquirectSize(int width, int height, const Limits& limits = {});

// Default vertical coverage in degrees: 360 * height / width, centered on the horizon.
// Exactly 2:1 yields 180. Returns 0 when the size is not usable.
double DefaultVerticalFovDeg(int width, int height);

// overrideDeg <= 0 uses DefaultVerticalFovDeg. Otherwise clamps to (0, 180].
double ResolveVerticalFovDeg(int width, int height, double overrideDeg);

struct Vec3 {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

// DirectX / texassemble face order: +X, -X, +Y, -Y, +Z, -Z.
// u,v are 0..1 across the face. Matches tools/bake_ibl.py face_uv_to_dir.
Vec3 CubeFaceDirection(int face, float u, float v);
void DirectionToFaceUv(float x, float y, float z, int* face, float* u, float* v);

// Debevec lat-long: +Z is the image center, +Y is toward the top, u increases toward +X.
// verticalFovDeg is the image's vertical coverage centered on the horizon; 180 matches
// a full equirect. Directions outside the covered band clamp to the nearest edge row.
void EquirectUvFromDirection(float x, float y, float z, float* u, float* v);
void EquirectUvFromDirection(float x, float y, float z, float verticalFovDeg, float* u, float* v);

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

bool ConvertEquirectToCube(
    const RgbaImage& image,
    int faceSize,
    double verticalFovDeg,
    CubeBgra* cube,
    std::string* error);

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
    // 0 means "auto" (derive coverage from the image aspect). Non-zero is the override
    // in degrees after clamping to (0, 180]. Included in the stamp so FOV changes rebake.
    double verticalFovDeg = 0;
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
    std::string warning;
};

// Reads pngFile, skips work when the stamp matches mtime, size, content hash, and FOV key.
// verticalFovOverrideDeg <= 0 uses 360*height/width. samples <= 0 writes the display
// cubemap and reports IBL as not matching the sky.
CookResult CookPanoramaFiles(
    const std::filesystem::path& pngFile,
    const CookPaths& outputs,
    DecodeImageFn decode,
    const Limits& limits,
    const IblConfig& ibl,
    double verticalFovOverrideDeg = 0.0);

}  // namespace panorama
