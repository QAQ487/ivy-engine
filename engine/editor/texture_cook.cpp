// texture_cook.cpp
// BC7 cook of package textures. See texture_cook.h.

#include "engine/editor/texture_cook.h"

#include "engine/core/texture_cook_policy.h"
#include "engine/core/utf8_path.h"

#include <Windows.h>
#include <objbase.h>

#include "DirectXTex.h"

#include <cstdio>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;
using namespace DirectX;

namespace texture_cook {
namespace {

// Same colour space the runtime uses for a loaded PNG (UNORM, no sRGB view), so a
// cooked texture samples exactly like the source image did.
constexpr DXGI_FORMAT kCookFormat = DXGI_FORMAT_BC7_UNORM;

// Returns S_OK when the .dds was written.
HRESULT CookOne(const fs::path& source, const fs::path& ddsPath, bool* sizeSkipped) {
    *sizeSkipped = false;

    TexMetadata metadata;
    ScratchImage image;
    HRESULT hr = LoadFromWICFile(source.c_str(), WIC_FLAGS_NONE, &metadata, image);
    if (FAILED(hr))
        return hr;

    if (metadata.dimension != TEX_DIMENSION_TEXTURE2D || metadata.arraySize != 1 ||
        !IsBlockCompressibleSize(metadata.width, metadata.height)) {
        *sizeSkipped = true;
        return S_OK;
    }

    if (metadata.format != DXGI_FORMAT_R8G8B8A8_UNORM) {
        ScratchImage converted;
        hr = Convert(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
                     DXGI_FORMAT_R8G8B8A8_UNORM, TEX_FILTER_DEFAULT, TEX_THRESHOLD_DEFAULT, converted);
        if (FAILED(hr))
            return hr;
        image = std::move(converted);
    }

    ScratchImage mips;
    hr = GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
                         TEX_FILTER_DEFAULT | TEX_FILTER_SEPARATE_ALPHA, 0, mips);
    if (FAILED(hr))
        return hr;

    ScratchImage compressed;
    hr = Compress(mips.GetImages(), mips.GetImageCount(), mips.GetMetadata(), kCookFormat,
                  TEX_COMPRESS_PARALLEL, TEX_THRESHOLD_DEFAULT, compressed);
    if (FAILED(hr))
        return hr;

    return SaveToDDSFile(compressed.GetImages(), compressed.GetImageCount(), compressed.GetMetadata(),
                         DDS_FLAGS_NONE, ddsPath.c_str());
}

std::string HrText(HRESULT hr) {
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%08lX", static_cast<unsigned long>(hr));
    return buf;
}

}  // namespace

bool CookPackageTextures(const fs::path& packageRom,
                         const std::function<void(const std::string&)>& log,
                         Stats* stats, std::string* error) {
    Stats local;
    Stats& s = stats ? *stats : local;
    s = Stats{};

    auto emit = [&](const std::string& line) {
        if (log)
            log(line);
    };

    std::error_code ec;
    if (!fs::is_directory(packageRom, ec)) {
        if (error)
            *error = "Texture cook: package rom folder missing.";
        return false;
    }

    // WIC needs COM on this (worker) thread. Balance only what this call initialized.
    const HRESULT coHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool coInitialized = SUCCEEDED(coHr);

    // Collect first so we do not delete files while the iterator walks the tree.
    std::vector<fs::path> sources;
    for (fs::recursive_directory_iterator it(packageRom, ec), end; it != end; it.increment(ec)) {
        if (ec)
            break;
        std::error_code fileEc;
        if (!it->is_regular_file(fileEc))
            continue;
        const fs::path rel = it->path().lexically_relative(packageRom);
        if (!ShouldCookRomTexture(PathToUtf8(rel)))
            continue;
        sources.push_back(it->path());
    }
    if (ec) {
        if (coInitialized)
            CoUninitialize();
        if (error)
            *error = "Texture cook: cannot walk package rom: " + ec.message();
        return false;
    }

    if (!sources.empty())
        emit("Cooking " + std::to_string(sources.size()) + " texture(s) to BC7...");

    for (const fs::path& source : sources) {
        const std::string name = PathToUtf8(source.lexically_relative(packageRom));
        fs::path ddsPath = source;
        ddsPath.replace_extension(L".dds");

        std::error_code existsEc;
        if (fs::exists(ddsPath, existsEc)) {
            ++s.skippedExisting;
            emit("  keep " + name + " (a .dds with the same name already exists)");
            continue;
        }

        bool sizeSkipped = false;
        const HRESULT hr = CookOne(source, ddsPath, &sizeSkipped);
        if (FAILED(hr)) {
            ++s.failed;
            std::error_code rmEc;
            fs::remove(ddsPath, rmEc);  // never leave a partial .dds that would shadow the source
            emit("  keep " + name + " (cook failed, hr=" + HrText(hr) + ")");
            continue;
        }
        if (sizeSkipped) {
            ++s.skippedSize;
            emit("  keep " + name + " (size is not a multiple of 4 or not a plain 2D image)");
            continue;
        }

        std::error_code rmEc;
        fs::remove(source, rmEc);
        if (rmEc) {
            // Both files in the package is wasteful but correct: the runtime prefers the source.
            ++s.failed;
            emit("  " + name + " cooked, but the source image could not be removed: " + rmEc.message());
            continue;
        }
        ++s.cooked;
    }

    if (coInitialized)
        CoUninitialize();

    emit("Texture cook: " + std::to_string(s.cooked) + " cooked, " +
         std::to_string(s.skippedSize) + " kept (size), " +
         std::to_string(s.skippedExisting) + " kept (existing .dds), " +
         std::to_string(s.failed) + " failed.");
    return true;
}

}  // namespace texture_cook
