// equirect_cubemap.cpp
// Lat-long panorama to a legacy DDS cubemap, plus the IBL mip layout from tools/bake_ibl.py.
// Display faces use pixel-center bilinear samples. IBL sampling matches bake_ibl.py
// (texel coordinate = saturate(uv) * (size - 1)).

#include "engine/rendering/environment/equirect_cubemap.h"

#include "engine/core/content_path.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace panorama {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr std::uint32_t kDdsCaps = 0x1u;
constexpr std::uint32_t kDdsHeight = 0x2u;
constexpr std::uint32_t kDdsWidth = 0x4u;
constexpr std::uint32_t kDdsPitch = 0x8u;
constexpr std::uint32_t kDdsPixelFormat = 0x1000u;
constexpr std::uint32_t kDdsMipCount = 0x20000u;
constexpr std::uint32_t kCapsTexture = 0x1000u;
constexpr std::uint32_t kCapsComplex = 0x8u;
constexpr std::uint32_t kCapsMipmap = 0x400000u;
constexpr std::uint32_t kCaps2CubeAll = 0xFE00u;

struct DVec3 {
    double x = 0;
    double y = 0;
    double z = 0;
};

bool IsPow2(int n) {
    return n > 0 && (n & (n - 1)) == 0;
}

int FloorPow2(int n) {
    if (n < 1)
        return 0;
    int p = 1;
    while (p <= n / 2)
        p *= 2;
    return p;
}

double ClampVerticalFovDeg(double degrees) {
    if (!(degrees > 0.0))
        return 0.0;
    if (degrees > 180.0)
        return 180.0;
    return degrees;
}

bool EndsWithPng(const std::string& path) {
    if (path.size() < 4)
        return false;
    const char* ext = path.c_str() + path.size() - 4;
    return (ext[0] == '.') &&
        (ext[1] == 'p' || ext[1] == 'P') &&
        (ext[2] == 'n' || ext[2] == 'N') &&
        (ext[3] == 'g' || ext[3] == 'G');
}

double Saturate(double v) {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

DVec3 Normalize(DVec3 v) {
    const double len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1e-8)
        return { 0.0, 1.0, 0.0 };
    const double inv = 1.0 / len;
    return { v.x * inv, v.y * inv, v.z * inv };
}

DVec3 FaceDirection(int face, double u, double v) {
    const double uc = 2.0 * u - 1.0;
    const double vc = 2.0 * v - 1.0;
    switch (face) {
    case 0: return Normalize({ 1.0, -vc, -uc });
    case 1: return Normalize({ -1.0, -vc, uc });
    case 2: return Normalize({ uc, 1.0, vc });
    case 3: return Normalize({ uc, -1.0, -vc });
    case 4: return Normalize({ uc, -vc, 1.0 });
    default: return Normalize({ -uc, -vc, -1.0 });
    }
}

void LatLongUv(double x, double y, double z, double verticalFovDeg, double* u, double* v) {
    const double theta = std::atan2(x, z);
    const double phi = std::acos(std::max(-1.0, std::min(1.0, y)));
    double uu = theta / (2.0 * kPi) + 0.5;
    if (uu < 0.0)
        uu += 1.0;
    if (uu > 1.0)
        uu = 1.0;
    *u = uu;

    const double fov = ClampVerticalFovDeg(verticalFovDeg);
    if (!(fov > 0.0)) {
        *v = Saturate(phi / kPi);
        return;
    }
    // Full-sphere v is phi/pi (0 at +Y, 1 at -Y). Map the band of height fov/180
    // centered on the horizon (v=0.5) onto the image, clamping outside rows.
    const double vFull = phi / kPi;
    double vv = (vFull - 0.5) * (180.0 / fov) + 0.5;
    *v = Saturate(vv);
}

void SampleEquirect(const std::uint8_t* rgba, int width, int height, double u, double v,
                     double* r, double* g, double* b, double* a) {
    const double sx = u * width - 0.5;
    const double sy = v * height - 0.5;
    double x = std::fmod(sx, static_cast<double>(width));
    if (x < 0.0)
        x += width;
    int x0 = static_cast<int>(std::floor(x));
    const double tx = x - x0;
    int x1 = x0 + 1;
    if (x0 >= width)
        x0 -= width;
    if (x1 >= width)
        x1 -= width;
    if (x0 < 0)
        x0 = 0;

    int y0 = 0;
    int y1 = 0;
    double ty = 0.0;
    if (sy < 0.0) {
        y0 = 0;
        y1 = 0;
        ty = 0.0;
    } else if (sy >= height - 1) {
        y0 = height - 1;
        y1 = height - 1;
        ty = 0.0;
    } else {
        y0 = static_cast<int>(std::floor(sy));
        ty = sy - y0;
        y1 = y0 + 1;
    }

    auto at = [&](int px, int py, int channel) -> double {
        const std::size_t i = (static_cast<std::size_t>(py) * width + px) * 4u + channel;
        return rgba[i];
    };
    auto lerp = [](double p, double q, double t) { return p * (1.0 - t) + q * t; };
    double out[4];
    for (int c = 0; c < 4; ++c) {
        const double row0 = lerp(at(x0, y0, c), at(x1, y0, c), tx);
        const double row1 = lerp(at(x0, y1, c), at(x1, y1, c), tx);
        out[c] = lerp(row0, row1, ty);
    }
    *r = out[0];
    *g = out[1];
    *b = out[2];
    *a = out[3];
}

int Quantize(double v) {
    const long rounded = std::lround(v);
    if (rounded < 0) return 0;
    if (rounded > 255) return 255;
    return static_cast<int>(rounded);
}

std::size_t FloatIndex(int size, int face, int x, int y) {
    return (static_cast<std::size_t>(face * size + y) * size + x) * 3u;
}

void SampleFloatFace(const FloatCube& cube, int face, double u, double v, double* r, double* g, double* b) {
    const int size = cube.size;
    const double x = Saturate(u) * (size - 1);
    const double y = Saturate(v) * (size - 1);
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, size - 1);
    const int y1 = std::min(y0 + 1, size - 1);
    const double tx = x - x0;
    const double ty = y - y0;
    auto at = [&](int px, int py) -> const float* {
        return cube.faces.data() + FloatIndex(size, face, px, py);
    };
    const float* c00 = at(x0, y0);
    const float* c10 = at(x1, y0);
    const float* c01 = at(x0, y1);
    const float* c11 = at(x1, y1);
    auto mix = [&](int channel) {
        const double row0 = c00[channel] * (1.0 - tx) + c10[channel] * tx;
        const double row1 = c01[channel] * (1.0 - tx) + c11[channel] * tx;
        return row0 * (1.0 - ty) + row1 * ty;
    };
    *r = mix(0);
    *g = mix(1);
    *b = mix(2);
}

void DirectionToCubeUv(double x, double y, double z, int* face, double* u, double* v) {
    const double ax = std::fabs(x);
    const double ay = std::fabs(y);
    const double az = std::fabs(z);
    int f = 0;
    double sc = 0;
    double tc = 0;
    double ma = 0;
    if (ax >= ay && ax >= az) {
        if (x >= 0.0) { f = 0; sc = -z; tc = -y; ma = ax; }
        else { f = 1; sc = z; tc = -y; ma = ax; }
    } else if (ay >= ax && ay >= az) {
        if (y >= 0.0) { f = 2; sc = x; tc = z; ma = ay; }
        else { f = 3; sc = x; tc = -z; ma = ay; }
    } else if (z >= 0.0) {
        f = 4; sc = x; tc = -y; ma = az;
    } else {
        f = 5; sc = -x; tc = -y; ma = az;
    }
    ma = std::max(ma, 1e-8);
    *face = f;
    *u = Saturate(0.5 * (sc / ma + 1.0));
    *v = Saturate(0.5 * (tc / ma + 1.0));
}

void SampleFloatCube(const FloatCube& cube, DVec3 d, double* r, double* g, double* b) {
    int face = 0;
    double u = 0;
    double v = 0;
    DirectionToCubeUv(d.x, d.y, d.z, &face, &u, &v);
    SampleFloatFace(cube, face, u, v, r, g, b);
}

void TangentBasis(DVec3 n, DVec3* tangent, DVec3* bitangent) {
    const DVec3 up = std::fabs(n.y) < 0.999 ? DVec3{ 0.0, 1.0, 0.0 } : DVec3{ 0.0, 0.0, 1.0 };
    *tangent = Normalize({ n.y * up.z - n.z * up.y, n.z * up.x - n.x * up.z, n.x * up.y - n.y * up.x });
    *bitangent = {
        n.y * tangent->z - n.z * tangent->y,
        n.z * tangent->x - n.x * tangent->z,
        n.x * tangent->y - n.y * tangent->x
    };
}

double RadicalInverse(std::uint32_t bits) {
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555u) << 1) | ((bits & 0xAAAAAAAAu) >> 1);
    bits = ((bits & 0x33333333u) << 2) | ((bits & 0xCCCCCCCCu) >> 2);
    bits = ((bits & 0x0F0F0F0Fu) << 4) | ((bits & 0xF0F0F0F0u) >> 4);
    bits = ((bits & 0x00FF00FFu) << 8) | ((bits & 0xFF00FF00u) >> 8);
    return static_cast<double>(bits) * 2.3283064365386963e-10;
}

void Hammersley(int i, int n, double* u, double* v) {
    *u = static_cast<double>(i) / static_cast<double>(n);
    *v = RadicalInverse(static_cast<std::uint32_t>(i));
}

DVec3 ImportanceGgx(double xi0, double xi1, DVec3 n, double roughness) {
    const double a = std::max(roughness * roughness, 1e-4);
    const double phi = 2.0 * kPi * xi0;
    const double cosTheta = std::sqrt((1.0 - xi1) / (1.0 + (a * a - 1.0) * xi1));
    const double sinTheta = std::sqrt(std::max(0.0, 1.0 - cosTheta * cosTheta));
    const double hx = std::cos(phi) * sinTheta;
    const double hy = std::sin(phi) * sinTheta;
    const double hz = cosTheta;
    DVec3 tangent;
    DVec3 bitangent;
    TangentBasis(n, &tangent, &bitangent);
    return Normalize({
        tangent.x * hx + bitangent.x * hy + n.x * hz,
        tangent.y * hx + bitangent.y * hy + n.y * hz,
        tangent.z * hx + bitangent.z * hy + n.z * hz
    });
}

void SetFloatTexel(FloatCube* cube, int face, int x, int y, double r, double g, double b) {
    float* p = cube->faces.data() + FloatIndex(cube->size, face, x, y);
    p[0] = static_cast<float>(r);
    p[1] = static_cast<float>(g);
    p[2] = static_cast<float>(b);
}

bool ReadAll(const std::filesystem::path& path, std::vector<std::uint8_t>* out) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    in.seekg(0, std::ios::end);
    const auto len = in.tellg();
    if (len < 0)
        return false;
    in.seekg(0, std::ios::beg);
    out->resize(static_cast<std::size_t>(len));
    if (!out->empty())
        in.read(reinterpret_cast<char*>(out->data()), static_cast<std::streamsize>(out->size()));
    return static_cast<bool>(in) || out->empty();
}

bool WriteAll(const std::filesystem::path& path, const std::uint8_t* data, std::size_t size) {
    std::error_code ec;
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    if (size)
        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(out);
}

std::int64_t MtimeSec(const std::filesystem::path& path) {
    std::error_code ec;
    const auto tp = std::filesystem::last_write_time(path, ec);
    if (ec)
        return 0;
    return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
}

bool NonEmptyFile(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec) && std::filesystem::file_size(path, ec) > 0;
}

void RemoveQuiet(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

void CubeBgraToFloat(const CubeBgra& src, FloatCube* dst) {
    dst->size = src.size;
    dst->faces.assign(static_cast<std::size_t>(6 * src.size * src.size * 3), 0.f);
    for (int face = 0; face < 6; ++face) {
        for (int y = 0; y < src.size; ++y) {
            for (int x = 0; x < src.size; ++x) {
                const std::size_t i = (static_cast<std::size_t>(face * src.size + y) * src.size + x) * 4u;
                const double b = src.faces[i] / 255.0;
                const double g = src.faces[i + 1] / 255.0;
                const double r = src.faces[i + 2] / 255.0;
                SetFloatTexel(dst, face, x, y, r, g, b);
            }
        }
    }
}

void ResampleFloatCube(const FloatCube& src, int dstSize, FloatCube* dst) {
    dst->size = dstSize;
    dst->faces.assign(static_cast<std::size_t>(6 * dstSize * dstSize * 3), 0.f);
    if (src.size == dstSize) {
        dst->faces = src.faces;
        return;
    }
    for (int face = 0; face < 6; ++face) {
        for (int y = 0; y < dstSize; ++y) {
            const double v = (y + 0.5) / dstSize;
            for (int x = 0; x < dstSize; ++x) {
                const double u = (x + 0.5) / dstSize;
                double r = 0, g = 0, b = 0;
                SampleFloatFace(src, face, u, v, &r, &g, &b);
                SetFloatTexel(dst, face, x, y, r, g, b);
            }
        }
    }
}

void FloatToLevel(const FloatCube& src, CubeLevel* dst) {
    dst->size = src.size;
    dst->faces.assign(static_cast<std::size_t>(6 * src.size * src.size * 4), 0);
    for (int face = 0; face < 6; ++face) {
        for (int y = 0; y < src.size; ++y) {
            for (int x = 0; x < src.size; ++x) {
                const float* p = src.faces.data() + FloatIndex(src.size, face, x, y);
                const std::size_t i = (static_cast<std::size_t>(face * src.size + y) * src.size + x) * 4u;
                dst->faces[i] = static_cast<std::uint8_t>(Quantize(Saturate(p[2]) * 255.0));
                dst->faces[i + 1] = static_cast<std::uint8_t>(Quantize(Saturate(p[1]) * 255.0));
                dst->faces[i + 2] = static_cast<std::uint8_t>(Quantize(Saturate(p[0]) * 255.0));
                dst->faces[i + 3] = 255;
            }
        }
    }
}

}  // namespace

double DefaultVerticalFovDeg(int width, int height) {
    if (width <= 0 || height <= 0)
        return 0.0;
    // height > width/2 means more than 180 deg of vertical coverage.
    if (height * 2 > width)
        return 0.0;
    return 360.0 * static_cast<double>(height) / static_cast<double>(width);
}

double ResolveVerticalFovDeg(int width, int height, double overrideDeg) {
    const double clamped = ClampVerticalFovDeg(overrideDeg);
    if (clamped > 0.0)
        return clamped;
    return DefaultVerticalFovDeg(width, height);
}

SizeCheck ValidateEquirectSize(int width, int height, const Limits& limits) {
    SizeCheck result;
    if (width <= 0 || height <= 0) {
        result.code = SizeCode::NonPositive;
        result.error = "Panorama size must be positive.";
        return result;
    }
    if (height * 2 > width) {
        result.code = SizeCode::BadAspect;
        const double assumed = 360.0 * static_cast<double>(height) / static_cast<double>(width);
        result.error = "Panorama vertical coverage exceeds 180 deg (height > width/2); got " +
            std::to_string(width) + "x" + std::to_string(height) +
            " (assumed " + std::to_string(assumed) + " deg).";
        return result;
    }

    const bool exactTwoToOne = (width == height * 2);
    if (exactTwoToOne) {
        // Preserve the original 2:1 rules exactly (odd height / non-PoT face rejected).
        if ((height % 2) != 0) {
            result.code = SizeCode::FaceNotPowerOfTwo;
            result.error = "Panorama face size (height/2) must be a power of two.";
            return result;
        }
        result.faceSize = height / 2;
        if (!IsPow2(result.faceSize)) {
            result.code = SizeCode::FaceNotPowerOfTwo;
            result.error = "Panorama face size " + std::to_string(result.faceSize) +
                " (height/2) must be a power of two.";
            return result;
        }
    } else {
        result.faceSize = FloorPow2(width / 4);
        if (result.faceSize < 1) {
            result.code = SizeCode::FaceNotPowerOfTwo;
            result.error = "Panorama face size (width/4) must be at least 1.";
            return result;
        }
    }

    if (width > limits.maxWidth || height > limits.maxHeight || result.faceSize > limits.maxFace) {
        result.code = SizeCode::Oversized;
        result.faceSize = 0;
        result.error = "Panorama " + std::to_string(width) + "x" + std::to_string(height) +
            " is oversized (max " + std::to_string(limits.maxWidth) + "x" +
            std::to_string(limits.maxHeight) + ", face max " + std::to_string(limits.maxFace) + ").";
        return result;
    }
    result.code = SizeCode::Ok;
    return result;
}

Vec3 CubeFaceDirection(int face, float u, float v) {
    const DVec3 d = FaceDirection(face, u, v);
    return { static_cast<float>(d.x), static_cast<float>(d.y), static_cast<float>(d.z) };
}

void DirectionToFaceUv(float x, float y, float z, int* face, float* u, float* v) {
    int f = 0;
    double uu = 0;
    double vv = 0;
    DirectionToCubeUv(x, y, z, &f, &uu, &vv);
    if (face) *face = f;
    if (u) *u = static_cast<float>(uu);
    if (v) *v = static_cast<float>(vv);
}

void EquirectUvFromDirection(float x, float y, float z, float* u, float* v) {
    EquirectUvFromDirection(x, y, z, 180.f, u, v);
}

void EquirectUvFromDirection(float x, float y, float z, float verticalFovDeg, float* u, float* v) {
    double uu = 0;
    double vv = 0;
    LatLongUv(x, y, z, verticalFovDeg, &uu, &vv);
    if (u) *u = static_cast<float>(uu);
    if (v) *v = static_cast<float>(vv);
}

bool ConvertEquirectToCube(
    const RgbaImage& image,
    int faceSize,
    double verticalFovDeg,
    CubeBgra* cube,
    std::string* error) {
    if (!cube) {
        if (error) *error = "Panorama convert is missing an output cube.";
        return false;
    }
    cube->size = 0;
    cube->faces.clear();
    if (faceSize <= 0 || image.width <= 0 || image.height <= 0 ||
        image.rgba.size() < static_cast<std::size_t>(image.width) * image.height * 4u) {
        if (error) *error = "Panorama image is incomplete.";
        return false;
    }
    const double fov = ClampVerticalFovDeg(verticalFovDeg);
    if (!(fov > 0.0)) {
        if (error) *error = "Panorama vertical FOV must be in (0, 180] degrees.";
        return false;
    }
    cube->size = faceSize;
    cube->faces.assign(static_cast<std::size_t>(6 * faceSize * faceSize * 4), 0);
    for (int face = 0; face < 6; ++face) {
        for (int y = 0; y < faceSize; ++y) {
            const double v = (y + 0.5) / faceSize;
            for (int x = 0; x < faceSize; ++x) {
                const double u = (x + 0.5) / faceSize;
                const DVec3 dir = FaceDirection(face, u, v);
                double eu = 0, ev = 0;
                LatLongUv(dir.x, dir.y, dir.z, fov, &eu, &ev);
                double r = 0, g = 0, b = 0, a = 0;
                SampleEquirect(image.rgba.data(), image.width, image.height, eu, ev, &r, &g, &b, &a);
                const std::size_t i = (static_cast<std::size_t>(face * faceSize + y) * faceSize + x) * 4u;
                cube->faces[i] = static_cast<std::uint8_t>(Quantize(b));
                cube->faces[i + 1] = static_cast<std::uint8_t>(Quantize(g));
                cube->faces[i + 2] = static_cast<std::uint8_t>(Quantize(r));
                cube->faces[i + 3] = static_cast<std::uint8_t>(Quantize(a));
            }
        }
    }
    if (error) error->clear();
    return true;
}

bool WriteLegacyCubemapDds(const CubeLevel* levels, int levelCount, std::vector<std::uint8_t>* dds, std::string* error) {
    if (!dds || !levels || levelCount < 1 || levels[0].size < 1) {
        if (error) *error = "DDS cubemap is missing a base face.";
        return false;
    }
    const int base = levels[0].size;
    for (int mip = 0; mip < levelCount; ++mip) {
        const int size = levels[mip].size;
        const std::size_t need = static_cast<std::size_t>(6 * size * size * 4);
        if (size < 1 || levels[mip].faces.size() < need) {
            if (error) *error = "DDS cubemap mip is incomplete.";
            return false;
        }
    }

    std::uint32_t flags = kDdsCaps | kDdsHeight | kDdsWidth | kDdsPitch | kDdsPixelFormat;
    std::uint32_t caps = kCapsTexture | kCapsComplex;
    if (levelCount > 1) {
        flags |= kDdsMipCount;
        caps |= kCapsMipmap;
    }

    std::vector<std::uint8_t> file(128, 0);
    auto put = [&](std::size_t offset, std::uint32_t value) {
        std::memcpy(file.data() + offset, &value, 4);
    };
    std::memcpy(file.data(), "DDS ", 4);
    put(4, 124);
    put(8, flags);
    put(12, static_cast<std::uint32_t>(base));
    put(16, static_cast<std::uint32_t>(base));
    put(20, static_cast<std::uint32_t>(base * 4));
    put(24, 0);
    put(28, static_cast<std::uint32_t>(levelCount));
    put(76, 32);
    put(80, 0x41);
    put(88, 32);
    put(92, 0x00FF0000u);
    put(96, 0x0000FF00u);
    put(100, 0x000000FFu);
    put(104, 0xFF000000u);
    put(108, caps);
    put(112, kCaps2CubeAll);

    for (int face = 0; face < 6; ++face) {
        for (int mip = 0; mip < levelCount; ++mip) {
            const int size = levels[mip].size;
            const std::size_t count = static_cast<std::size_t>(size * size * 4);
            const std::size_t begin = static_cast<std::size_t>(face) * count;
            file.insert(file.end(), levels[mip].faces.begin() + static_cast<std::ptrdiff_t>(begin),
                levels[mip].faces.begin() + static_cast<std::ptrdiff_t>(begin + count));
        }
    }
    *dds = std::move(file);
    if (error) error->clear();
    return true;
}

IblConfig ProductionIblConfig() {
    return {};
}

void BakeIrradiance(const FloatCube& src, int samples, int dstSize, FloatCube* dst) {
    if (!dst)
        return;
    dst->size = dstSize;
    dst->faces.assign(static_cast<std::size_t>(6 * dstSize * dstSize * 3), 0.f);
    if (src.size < 1 || samples < 1 || dstSize < 1)
        return;
    for (int face = 0; face < 6; ++face) {
        for (int y = 0; y < dstSize; ++y) {
            const double v = (y + 0.5) / dstSize;
            for (int x = 0; x < dstSize; ++x) {
                const double u = (x + 0.5) / dstSize;
                const DVec3 n = FaceDirection(face, u, v);
                DVec3 tangent;
                DVec3 bitangent;
                TangentBasis(n, &tangent, &bitangent);
                double accR = 0, accG = 0, accB = 0, weight = 0;
                for (int i = 0; i < samples; ++i) {
                    double xi0 = 0, xi1 = 0;
                    Hammersley(i, samples, &xi0, &xi1);
                    const double cosTheta = std::sqrt(1.0 - xi1);
                    const double sinTheta = std::sqrt(xi1);
                    const double phi = 2.0 * kPi * xi0;
                    const double lx = std::cos(phi) * sinTheta;
                    const double ly = std::sin(phi) * sinTheta;
                    const double lz = cosTheta;
                    const DVec3 l = Normalize({
                        tangent.x * lx + bitangent.x * ly + n.x * lz,
                        tangent.y * lx + bitangent.y * ly + n.y * lz,
                        tangent.z * lx + bitangent.z * ly + n.z * lz
                    });
                    const double ndotl = Saturate(n.x * l.x + n.y * l.y + n.z * l.z);
                    if (ndotl <= 0.0)
                        continue;
                    double r = 0, g = 0, b = 0;
                    SampleFloatCube(src, l, &r, &g, &b);
                    accR += r * ndotl;
                    accG += g * ndotl;
                    accB += b * ndotl;
                    weight += ndotl;
                }
                const double inv = 1.0 / std::max(weight, 1e-5);
                SetFloatTexel(dst, face, x, y, accR * inv, accG * inv, accB * inv);
            }
        }
    }
}

void BakePrefilter(const FloatCube& src, int samples, int baseSize, int mipCount, std::vector<FloatCube>* mips) {
    if (!mips)
        return;
    mips->clear();
    if (src.size < 1 || baseSize < 1 || mipCount < 1)
        return;
    int size = baseSize;
    mips->reserve(static_cast<std::size_t>(mipCount));
    for (int mip = 0; mip < mipCount; ++mip) {
        const double roughness = (mipCount == 1) ? 0.0 : static_cast<double>(mip) / (mipCount - 1);
        FloatCube cube;
        cube.size = size;
        cube.faces.assign(static_cast<std::size_t>(6 * size * size * 3), 0.f);
        for (int face = 0; face < 6; ++face) {
            for (int y = 0; y < size; ++y) {
                const double v = (y + 0.5) / size;
                for (int x = 0; x < size; ++x) {
                    const double u = (x + 0.5) / size;
                    const DVec3 n = FaceDirection(face, u, v);
                    if (roughness < 0.04 || samples < 1) {
                        double r = 0, g = 0, b = 0;
                        SampleFloatCube(src, n, &r, &g, &b);
                        SetFloatTexel(&cube, face, x, y, r, g, b);
                        continue;
                    }
                    double accR = 0, accG = 0, accB = 0, weight = 0;
                    for (int i = 0; i < samples; ++i) {
                        double xi0 = 0, xi1 = 0;
                        Hammersley(i, samples, &xi0, &xi1);
                        const DVec3 h = ImportanceGgx(xi0, xi1, n, roughness);
                        const double vdotn = n.x * n.x + n.y * n.y + n.z * n.z;
                        const double vdoth = h.x * n.x + h.y * n.y + h.z * n.z;
                        const DVec3 l = Normalize({
                            2.0 * vdoth * h.x - n.x * vdotn,
                            2.0 * vdoth * h.y - n.y * vdotn,
                            2.0 * vdoth * h.z - n.z * vdotn
                        });
                        const double ndotl = Saturate(n.x * l.x + n.y * l.y + n.z * l.z);
                        if (ndotl <= 0.0)
                            continue;
                        double r = 0, g = 0, b = 0;
                        SampleFloatCube(src, l, &r, &g, &b);
                        accR += r * ndotl;
                        accG += g * ndotl;
                        accB += b * ndotl;
                        weight += ndotl;
                    }
                    const double inv = 1.0 / std::max(weight, 1e-5);
                    SetFloatTexel(&cube, face, x, y, accR * inv, accG * inv, accB * inv);
                }
            }
        }
        mips->push_back(std::move(cube));
        size = std::max(size / 2, 1);
    }
}

std::uint64_t Fnv1a64(const std::uint8_t* data, std::size_t size) {
    std::uint64_t hash = 14695981039346656037ull;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= data ? data[i] : 0;
        hash *= 1099511628211ull;
    }
    return hash;
}

bool SameVerticalFovKey(double a, double b) {
    if (a <= 0.0 && b <= 0.0)
        return true;
    if (a <= 0.0 || b <= 0.0)
        return false;
    return std::fabs(a - b) <= 1e-6;
}

bool CacheIsFresh(const FileStamp& source, const FileStamp& recorded) {
    return source.size > 0 &&
        source.mtimeSec == recorded.mtimeSec &&
        source.size == recorded.size &&
        source.contentHash == recorded.contentHash &&
        SameVerticalFovKey(source.verticalFovDeg, recorded.verticalFovDeg);
}

std::string FormatCacheStamp(const FileStamp& stamp) {
    std::ostringstream out;
    out << "ivy-panorama-stamp 2\n"
        << "mtime " << stamp.mtimeSec << "\n"
        << "size " << stamp.size << "\n"
        << "fnv1a64 " << std::hex << std::setw(16) << std::setfill('0') << stamp.contentHash << "\n"
        << std::dec;
    if (stamp.verticalFovDeg <= 0.0)
        out << "vfov auto\n";
    else
        out << "vfov " << std::setprecision(17) << stamp.verticalFovDeg << "\n";
    return out.str();
}

bool ParseCacheStamp(const std::string& text, FileStamp* stamp) {
    if (!stamp)
        return false;
    stamp->verticalFovDeg = 0;
    std::istringstream in(text);
    std::string tag;
    std::string version;
    std::string key;
    in >> tag >> version;
    if (!in || tag != "ivy-panorama-stamp" || (version != "1" && version != "2"))
        return false;
    in >> key;
    if (key != "mtime")
        return false;
    in >> stamp->mtimeSec;
    in >> key;
    if (key != "size")
        return false;
    in >> stamp->size;
    in >> key;
    if (key != "fnv1a64")
        return false;
    in >> std::hex >> stamp->contentHash;
    if (version == "1")
        return static_cast<bool>(in);
    in >> std::dec >> key;
    if (key != "vfov")
        return false;
    std::string fovToken;
    in >> fovToken;
    if (!in)
        return false;
    if (fovToken == "auto") {
        stamp->verticalFovDeg = 0;
        return true;
    }
    try {
        stamp->verticalFovDeg = ClampVerticalFovDeg(std::stod(fovToken));
    } catch (...) {
        return false;
    }
    return stamp->verticalFovDeg > 0.0;
}

std::string PanoramaCacheRomPath(const std::string& pngRomPath, const char* kind) {
    std::string normalized;
    if (!content_path::IsSafeRomPath(pngRomPath, &normalized) || !EndsWithPng(normalized))
        return {};
    const std::string rest = normalized.substr(4, normalized.size() - 4 - 4);
    std::string slug;
    slug.reserve(rest.size() + 8);
    for (char c : rest) {
        if (c == '/')
            slug += "__";
        else
            slug.push_back(c);
    }
    const char* suffix = nullptr;
    if (kind && std::strcmp(kind, "cube") == 0)
        suffix = ".dds";
    else if (kind && std::strcmp(kind, "irradiance") == 0)
        suffix = ".irradiance.dds";
    else if (kind && std::strcmp(kind, "prefilter") == 0)
        suffix = ".prefilter.dds";
    else if (kind && std::strcmp(kind, "stamp") == 0)
        suffix = ".stamp";
    else
        return {};
    return "rom/skybox/cache/" + slug + suffix;
}

CookResult CookPanoramaFiles(
    const std::filesystem::path& pngFile,
    const CookPaths& outputs,
    DecodeImageFn decode,
    const Limits& limits,
    const IblConfig& ibl,
    double verticalFovOverrideDeg) {
    CookResult result;
    std::vector<std::uint8_t> bytes;
    if (!ReadAll(pngFile, &bytes)) {
        result.error = "Panorama file not found: " + pngFile.string();
        return result;
    }

    const double fovKey = ClampVerticalFovDeg(verticalFovOverrideDeg);
    FileStamp now;
    now.mtimeSec = MtimeSec(pngFile);
    now.size = bytes.size();
    now.contentHash = Fnv1a64(bytes.data(), bytes.size());
    now.verticalFovDeg = fovKey;
    std::vector<std::uint8_t> stampBytes;
    FileStamp recorded;
    if (ReadAll(outputs.stamp, &stampBytes) &&
        ParseCacheStamp(std::string(stampBytes.begin(), stampBytes.end()), &recorded) &&
        CacheIsFresh(now, recorded) &&
        NonEmptyFile(outputs.cubemap) &&
        NonEmptyFile(outputs.irradiance) &&
        NonEmptyFile(outputs.prefilter)) {
        result.ok = true;
        result.iblReady = true;
        return result;
    }
    RemoveQuiet(outputs.stamp);

    if (!decode) {
        result.error = "decode failed";
        return result;
    }
    DecodedImage image;
    std::string decodeError;
    if (!decode(bytes.data(), bytes.size(), &image, &decodeError)) {
        result.error = decodeError.empty() ? "decode failed" : decodeError;
        return result;
    }
    const SizeCheck size = ValidateEquirectSize(image.width, image.height, limits);
    if (size.code != SizeCode::Ok) {
        result.error = size.error;
        return result;
    }
    const std::size_t need = static_cast<std::size_t>(image.width) * image.height * 4u;
    if (image.rgba.size() < need) {
        result.error = "decode failed";
        return result;
    }

    const double resolvedFov = ResolveVerticalFovDeg(image.width, image.height, verticalFovOverrideDeg);
    if (!(resolvedFov > 0.0)) {
        result.error = "Panorama vertical FOV must be in (0, 180] degrees.";
        return result;
    }
    if (image.width != image.height * 2) {
        std::ostringstream warn;
        warn << "Panorama " << image.width << "x" << image.height
             << " is not 2:1; assuming " << resolvedFov
             << " deg vertical coverage (width = 360 deg), centered on the horizon.";
        result.warning = warn.str();
    }

    RgbaImage rgba;
    rgba.width = image.width;
    rgba.height = image.height;
    rgba.rgba = std::move(image.rgba);
    CubeBgra cube;
    std::string convertError;
    if (!ConvertEquirectToCube(rgba, size.faceSize, resolvedFov, &cube, &convertError)) {
        result.error = convertError.empty() ? "Panorama convert failed." : convertError;
        return result;
    }
    CubeLevel display;
    display.size = cube.size;
    display.faces = cube.faces;
    std::vector<std::uint8_t> dds;
    std::string ddsError;
    if (!WriteLegacyCubemapDds(&display, 1, &dds, &ddsError) ||
        !WriteAll(outputs.cubemap, dds.data(), dds.size())) {
        result.error = ddsError.empty() ? "Failed to write the cooked cubemap." : ddsError;
        return result;
    }

    if (ibl.samples < 1 || ibl.sourceSize < 1 || ibl.irradianceSize < 1 ||
        ibl.prefilterSize < 1 || ibl.prefilterMips < 1) {
        RemoveQuiet(outputs.irradiance);
        RemoveQuiet(outputs.prefilter);
        RemoveQuiet(outputs.stamp);
        result.ok = true;
        result.rebaked = true;
        result.error = "IBL not matching sky: irradiance and prefilter were not baked.";
        return result;
    }

    try {
        FloatCube linear;
        CubeBgraToFloat(cube, &linear);
        FloatCube source;
        ResampleFloatCube(linear, ibl.sourceSize, &source);
        FloatCube irradiance;
        BakeIrradiance(source, ibl.samples, ibl.irradianceSize, &irradiance);
        std::vector<FloatCube> mips;
        BakePrefilter(source, ibl.samples, ibl.prefilterSize, ibl.prefilterMips, &mips);
        CubeLevel irrLevel;
        FloatToLevel(irradiance, &irrLevel);
        std::vector<CubeLevel> preLevels(mips.size());
        for (std::size_t i = 0; i < mips.size(); ++i)
            FloatToLevel(mips[i], &preLevels[i]);
        std::vector<std::uint8_t> irrDds;
        std::vector<std::uint8_t> preDds;
        if (!WriteLegacyCubemapDds(&irrLevel, 1, &irrDds, &ddsError) ||
            !WriteLegacyCubemapDds(preLevels.data(), static_cast<int>(preLevels.size()), &preDds, &ddsError) ||
            !WriteAll(outputs.irradiance, irrDds.data(), irrDds.size()) ||
            !WriteAll(outputs.prefilter, preDds.data(), preDds.size())) {
            RemoveQuiet(outputs.stamp);
            result.ok = true;
            result.rebaked = true;
            result.error = "IBL not matching sky: failed to write irradiance or prefilter.";
            return result;
        }
    } catch (const std::exception& ex) {
        RemoveQuiet(outputs.stamp);
        result.ok = true;
        result.rebaked = true;
        result.error = std::string("IBL not matching sky: ") + ex.what();
        return result;
    }

    const std::string stamp = FormatCacheStamp(now);
    if (!WriteAll(outputs.stamp, reinterpret_cast<const std::uint8_t*>(stamp.data()), stamp.size())) {
        result.ok = true;
        result.rebaked = true;
        result.error = "IBL not matching sky: failed to write the cache stamp.";
        return result;
    }
    result.ok = true;
    result.rebaked = true;
    result.iblReady = true;
    return result;
}

}  // namespace panorama
