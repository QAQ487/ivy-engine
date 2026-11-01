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

std::filesystem::path ReadableFileCurrent(std::string_view path) {
    return ReadableFile(path, CurrentRomRoots());
}

std::filesystem::path WritableFileCurrent(std::string_view path) {
    return WritableFile(path, CurrentRomRoots());
}

namespace {

std::string WideToUtf8(const wchar_t* text) {
    if (!text || !text[0])
        return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1)
        return {};
    std::string utf8(static_cast<size_t>(length - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8.data(), length, nullptr, nullptr);
    return utf8;
}

}  // namespace

std::wstring ReadableWideCurrent(const wchar_t* path) {
    if (!path || !path[0])
        return {};
    const std::string utf8 = WideToUtf8(path);
    if (!IsRomShapedPath(utf8))
        return path;
    const std::filesystem::path resolved = ResolveRomForRead(utf8, CurrentRomRoots());
    if (resolved.empty())
        return path;
    return resolved.wstring();
}

}  // namespace content_path
