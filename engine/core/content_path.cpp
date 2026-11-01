// content_path.cpp
// Process-wide rom read/write roots: project cwd, then <exe-dir>/resources.

#include "engine/core/content_path.h"

namespace content_path {

std::filesystem::path ExecutableDirectory() {
    wchar_t buffer[32768] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, 32768);
    if (length == 0 || length >= 32768)
        return {};
    return std::filesystem::path(buffer).parent_path();
}

RomRoots CurrentRomRoots() {
    std::error_code ec;
    const std::filesystem::path cwd = std::filesystem::current_path(ec);
    return RomRoots{
        ec ? std::filesystem::path{} : cwd,
        ExecutableDirectory() / "resources",
    };
}

std::filesystem::path ResolveRomForReadCurrent(std::string_view path) {
    return ResolveRomForRead(path, CurrentRomRoots());
}

std::filesystem::path ResolveRomForWriteCurrent(std::string_view path) {
    return ResolveRomForWrite(path, CurrentRomRoots());
}

}  // namespace content_path
