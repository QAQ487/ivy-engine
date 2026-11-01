// panorama_sky.h
// Cook an equirectangular PNG into the project rom cache and report IBL state.
// DDS skybox paths are left untouched.

#pragma once

#include <filesystem>
#include <string>

namespace panorama {

struct Prep {
    bool isPanorama = false;
    bool ok = false;
    bool iblReady = false;
    std::string sourceRom;
    std::string cubemapRom;
    std::string irradianceRom;
    std::string prefilterRom;
    std::string error;
};

bool IsEquirectSkyPath(const std::string& path);
bool SameRomPath(const std::string& a, const std::string& b);

// Writes to OutputDebugString and appends content_hot_reload.log (retrievable local log).
void LogDiagnostic(const std::string& message);

// Uses the process project/resources roots. DDS paths return isPanorama false.
Prep PrepareSky(const std::string& skyboxRomPath);
const Prep& CurrentPrep();

// Cook every skyboxCubemap PNG named under projectRoot before packaging.
// Writes only into that project's rom/. resourcesRoot is the read fallback.
bool CookProjectPanoramas(const std::filesystem::path& projectRoot,
                          const std::filesystem::path& resourcesRoot,
                          std::string* error);

}  // namespace panorama
