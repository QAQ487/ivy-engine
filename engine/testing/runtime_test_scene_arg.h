// runtime_test_scene_arg.h
// Pure validation for IvyEngineRuntime --scene <rom/...json> (no I/O).

#pragma once

#include <string>
#include <string_view>

namespace ivy::testing {

// Accepts rom/-relative paths only: no absolute paths, no '.'/'..' segments,
// must end with .json (case-insensitive). Writes normalized rom/... form.
inline bool ValidateRuntimeTestSceneArg(std::string_view path, std::string* normalized,
    std::string* error) {
    auto fail = [&](const char* msg) {
        if (error) *error = msg;
        if (normalized) normalized->clear();
        return false;
    };
    if (path.empty())
        return fail("empty scene path");
    if (path.size() > 512)
        return fail("scene path too long");

    std::string n;
    n.reserve(path.size());
    for (unsigned char c : path) {
        if (c < 0x20)
            return fail("scene path contains control characters");
        n.push_back(c == '\\' ? '/' : static_cast<char>(c));
    }
    if (n[0] == '/')
        return fail("scene path must be rom/-relative (not absolute)");
    if (n.size() >= 2 &&
        ((n[0] >= 'A' && n[0] <= 'Z') || (n[0] >= 'a' && n[0] <= 'z')) &&
        n[1] == ':')
        return fail("scene path must be rom/-relative (not absolute)");
    if (n.size() < 5 || n.compare(0, 4, "rom/") != 0)
        return fail("scene path must start with rom/");

    std::size_t start = 0;
    while (start <= n.size()) {
        const std::size_t slash = n.find('/', start);
        const std::size_t count =
            (slash == std::string::npos) ? std::string::npos : slash - start;
        const std::string seg = n.substr(start, count);
        if (seg.empty() || seg == "." || seg == "..")
            return fail("scene path must not contain '.' or '..' segments");
        if (seg.front() == '.' || seg.back() == '.' || seg.back() == ' ')
            return fail("scene path segment invalid");
        if (seg.find_first_of("<>:\"|?*") != std::string::npos)
            return fail("scene path contains illegal characters");
        if (slash == std::string::npos)
            break;
        start = slash + 1;
        if (start == n.size())
            return fail("scene path must not end with '/'");
    }

    if (n.size() < 5)
        return fail("scene path too short");
    const std::string ext = n.substr(n.size() - 5);
    auto lower = [](char c) {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    };
    if (ext.size() != 5 || lower(ext[0]) != '.' || lower(ext[1]) != 'j' ||
        lower(ext[2]) != 's' || lower(ext[3]) != 'o' || lower(ext[4]) != 'n')
        return fail("scene path must end with .json");

    if (normalized) *normalized = std::move(n);
    if (error) error->clear();
    return true;
}

} // namespace ivy::testing
