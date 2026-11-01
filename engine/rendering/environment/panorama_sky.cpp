// panorama_sky.cpp
// Windows cook path: DirectXTex WIC decodes the PNG, equirect_cubemap writes the DDS.
// Runtime and editor share this file. There is no texassemble or Python dependency.

#include "panorama_sky.h"

#include "engine/rendering/environment/equirect_cubemap.h"
#include "engine/core/content_path.h"
#include "engine/debug_draw/debug_ofstream.h"
#include "engine/scene/render_settings.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif
#include "DirectXTex.h"
#include "libraries/nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>

namespace panorama {
namespace {

Prep g_prep;

bool EndsWithPng(const std::string& path) {
    if (path.size() < 4)
        return false;
    const char* ext = path.c_str() + path.size() - 4;
    return ext[0] == '.' &&
        (ext[1] == 'p' || ext[1] == 'P') &&
        (ext[2] == 'n' || ext[2] == 'N') &&
        (ext[3] == 'g' || ext[3] == 'G');
}

std::string LowerCopy(std::string value) {
    for (char& c : value)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

bool DecodeWic(const std::uint8_t* bytes, std::size_t size, DecodedImage* image, std::string* error) {
    if (!image) {
        if (error) *error = "decode failed";
        return false;
    }
    image->width = 0;
    image->height = 0;
    image->rgba.clear();
    DirectX::ScratchImage scratch;
    DirectX::TexMetadata metadata{};
    const HRESULT hr = DirectX::LoadFromWICMemory(bytes, size, DirectX::WIC_FLAGS_NONE, &metadata, scratch);
    if (FAILED(hr)) {
        if (error) {
            char buf[80];
            std::snprintf(buf, sizeof(buf), "decode failed (hr=0x%08lX)", static_cast<unsigned long>(hr));
            *error = buf;
        }
        return false;
    }
    const DirectX::Image* src = scratch.GetImage(0, 0, 0);
    if (!src || !src->pixels) {
        if (error) *error = "decode failed";
        return false;
    }
    DirectX::ScratchImage converted;
    if (src->format != DXGI_FORMAT_R8G8B8A8_UNORM) {
        const HRESULT convertHr = DirectX::Convert(
            *src, DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, 0.5f, converted);
        if (FAILED(convertHr) || !converted.GetImage(0, 0, 0)) {
            if (error) *error = "decode failed";
            return false;
        }
        src = converted.GetImage(0, 0, 0);
    }
    if (src->width == 0 || src->height == 0 || src->width > 16384 || src->height > 16384) {
        if (error) *error = "decode failed";
        return false;
    }
    image->width = static_cast<int>(src->width);
    image->height = static_cast<int>(src->height);
    image->rgba.resize(static_cast<std::size_t>(image->width) * image->height * 4u);
    for (int y = 0; y < image->height; ++y) {
        std::memcpy(image->rgba.data() + static_cast<std::size_t>(y) * image->width * 4u,
            src->pixels + static_cast<std::size_t>(y) * src->rowPitch,
            static_cast<std::size_t>(image->width) * 4u);
    }
    return true;
}

Prep CookOne(const std::string& skyboxRomPath, const content_path::RomRoots& roots) {
    Prep prep;
    std::string normalized;
    if (!EndsWithPng(skyboxRomPath)) {
        prep.ok = true;
        return prep;
    }
    prep.isPanorama = true;
    if (!content_path::IsSafeRomPath(skyboxRomPath, &normalized)) {
        prep.error = "Panorama path must be a safe rom/ PNG.";
        return prep;
    }
    prep.sourceRom = normalized;
    prep.cubemapRom = PanoramaCacheRomPath(normalized, "cube");
    prep.irradianceRom = PanoramaCacheRomPath(normalized, "irradiance");
    prep.prefilterRom = PanoramaCacheRomPath(normalized, "prefilter");
    const std::string stampRom = PanoramaCacheRomPath(normalized, "stamp");
    if (prep.cubemapRom.empty() || stampRom.empty()) {
        prep.error = "Panorama path must be a safe rom/ PNG.";
        return prep;
    }

    const std::filesystem::path source = content_path::ResolveRomForRead(normalized, roots);
    if (source.empty()) {
        prep.error = "Panorama file not found: " + normalized;
        return prep;
    }
    CookPaths outputs;
    outputs.cubemap = content_path::ResolveRomForWrite(prep.cubemapRom, roots);
    outputs.irradiance = content_path::ResolveRomForWrite(prep.irradianceRom, roots);
    outputs.prefilter = content_path::ResolveRomForWrite(prep.prefilterRom, roots);
    outputs.stamp = content_path::ResolveRomForWrite(stampRom, roots);
    if (outputs.cubemap.empty() || outputs.stamp.empty()) {
        prep.error = "No project rom to write the cooked cubemap.";
        return prep;
    }

    const CookResult cooked = CookPanoramaFiles(source, outputs, DecodeWic, {}, ProductionIblConfig());
    prep.ok = cooked.ok;
    prep.iblReady = cooked.iblReady;
    prep.error = cooked.error;
    if (cooked.rebaked)
        hal::dout << "Panorama cooked " << prep.cubemapRom << " from " << normalized << std::endl;
    if (prep.ok && prep.iblReady) {
        const std::string lut = RenderSettings_GetConst().iblBrdfLutPath;
        if (content_path::ResolveRomForRead(lut, roots).empty()) {
            prep.iblReady = false;
            prep.error = "IBL not matching sky: missing " + lut;
        }
    }
    return prep;
}

void CollectSkyPng(const nlohmann::json& json, std::set<std::string>* out) {
    if (json.is_object()) {
        const auto it = json.find("skyboxCubemap");
        if (it != json.end() && it->is_string()) {
            const std::string value = it->get<std::string>();
            if (IsEquirectSkyPath(value))
                out->insert(value);
        }
        for (auto item = json.begin(); item != json.end(); ++item)
            CollectSkyPng(item.value(), out);
    } else if (json.is_array()) {
        for (const auto& item : json)
            CollectSkyPng(item, out);
    }
}

}  // namespace

bool IsEquirectSkyPath(const std::string& path) {
    return EndsWithPng(path);
}

bool SameRomPath(const std::string& a, const std::string& b) {
    std::string left;
    std::string right;
    if (!content_path::IsSafeRomPath(a, &left) || !content_path::IsSafeRomPath(b, &right))
        return false;
    return LowerCopy(left) == LowerCopy(right);
}

Prep PrepareSky(const std::string& skyboxRomPath) {
    g_prep = CookOne(skyboxRomPath, content_path::CurrentRomRoots());
    return g_prep;
}

const Prep& CurrentPrep() {
    return g_prep;
}

bool CookProjectPanoramas(const std::filesystem::path& projectRoot,
                          const std::filesystem::path& resourcesRoot,
                          std::string* error) {
    const content_path::RomRoots roots{ projectRoot, resourcesRoot };
    std::set<std::string> pngs;
    const std::filesystem::path rom = projectRoot / "rom";
    std::error_code ec;
    if (std::filesystem::is_directory(rom, ec)) {
        for (std::filesystem::recursive_directory_iterator it(rom, ec), end; it != end; it.increment(ec)) {
            if (ec || !it->is_regular_file(ec))
                continue;
            std::string ext = it->path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (ext != ".json")
                continue;
            std::ifstream file(it->path(), std::ios::binary);
            if (!file)
                continue;
            std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            if (text.size() >= 3 &&
                static_cast<unsigned char>(text[0]) == 0xEF &&
                static_cast<unsigned char>(text[1]) == 0xBB &&
                static_cast<unsigned char>(text[2]) == 0xBF) {
                text.erase(0, 3);
            }
            try {
                CollectSkyPng(nlohmann::json::parse(text), &pngs);
            } catch (...) {
            }
        }
    }
    for (const std::string& png : pngs) {
        const Prep prep = CookOne(png, roots);
        if (!prep.ok) {
            if (error)
                *error = prep.sourceRom.empty() ? prep.error : prep.sourceRom + ": " + prep.error;
            return false;
        }
    }
    return true;
}

}  // namespace panorama
