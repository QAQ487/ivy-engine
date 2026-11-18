// package_assets.cpp
// Reachability scan for playable packages. See package_assets.h.

#include "engine/editor/package_assets.h"

#include "engine/core/utf8_path.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <fstream>
#include <iterator>
#include <map>
#include <system_error>

namespace fs = std::filesystem;

namespace package_assets {
namespace {

std::string Lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

bool ReadAll(const fs::path& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

std::string Extension(const std::string& rel) {
    const std::size_t slash = rel.find_last_of('/');
    const std::size_t dot = rel.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return {};
    return Lower(rel.substr(dot));
}

std::string BaseName(const std::string& rel) {
    const std::size_t slash = rel.find_last_of('/');
    return slash == std::string::npos ? rel : rel.substr(slash + 1);
}

bool StartsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

void TrimEnd(std::string& s, const char* chars) {
    while (!s.empty() && std::string(chars).find(s.back()) != std::string::npos)
        s.pop_back();
}

// Ends a "rom/..." token. Spaces stay inside: names like "Y Bot_white.fbx" exist, and a
// token that swallowed following words is cut back at spaces during resolution.
bool IsTerminator(unsigned char c) {
    return c < 0x20 || c == '"' || c == '\'' || c == '<' || c == '>' || c == '|' || c == '*' ||
           c == '?' || c == ':' || c == ';' || c == '=' || c == '{' || c == '}' || c == '$' ||
           c == ',';
}

// Lower case, '\\' -> '/', repeated '/' collapsed (JSON-escaped "\\\\" becomes one '/').
std::string NormalizeText(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        char c = ch == '\\' ? '/' : ch;
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
        if (c == '/' && !out.empty() && out.back() == '/')
            continue;
        out.push_back(c);
    }
    return out;
}

class Scanner {
public:
    Scanner(const Options& options, Result& result) : options_(options), result_(result) {}

    void Run() {
        Enumerate();

        // Files every package needs whatever the scenes say.
        for (const char* path : {"rom/scene/default_scene.txt", "rom/tags.json", "rom/editor/ui_prefs.json",
                                 "rom/shader", "rom/skybox/cache"})
            ResolveAndAdd(path);

        for (const std::string& scene : options_.rootScenes) {
            if (scene.empty())
                continue;
            if (!ResolveAndAdd("rom/scene/" + scene))
                result_.warnings.push_back("Default scene not found: rom/scene/" + scene);
        }
        for (std::string entry : options_.alwaysInclude) {
            std::string token = NormalizeText(entry);
            if (!StartsWith(token, "rom/"))
                token = "rom/" + token;
            if (!ResolveAndAdd(token))
                result_.warnings.push_back("build.alwaysInclude matches no file: " + entry);
        }

        for (const fs::path& file : options_.scanFiles)
            ScanOutsideFile(file);

        while (!queue_.empty()) {
            const std::string rel = queue_.front();
            queue_.pop_front();
            ScanRomFile(rel);
        }

        for (const auto& entry : lowerToActual_) {
            const std::string& rel = entry.second;
            std::error_code ec;
            const auto size = fs::file_size(options_.romRoot / PathFromUtf8(rel), ec);
            const unsigned long long bytes = ec ? 0 : static_cast<unsigned long long>(size);
            if (result_.included.count(rel)) {
                result_.includedBytes += bytes;
            } else {
                result_.excluded.push_back(rel);
                result_.excludedBytes += bytes;
            }
        }
        std::sort(result_.excluded.begin(), result_.excluded.end());
    }

private:
    void Enumerate() {
        std::error_code ec;
        if (!fs::is_directory(options_.romRoot, ec))
            return;
        for (fs::recursive_directory_iterator it(options_.romRoot, ec), end; it != end; it.increment(ec)) {
            if (ec)
                break;
            std::error_code fileEc;
            if (!it->is_regular_file(fileEc))
                continue;
            const std::string rel = PathToUtf8(it->path().lexically_relative(options_.romRoot));
            if (rel.empty())
                continue;
            const std::string lower = Lower(rel);
            lowerToActual_.emplace(lower, rel);
            byBaseName_[BaseName(lower)].push_back(rel);
            if (StartsWith(lower, "scene/") && Extension(lower) == ".json")
                sceneNames_.push_back({lower.substr(6), BaseName(lower), rel});
        }
    }

    void Add(const std::string& rel) {
        if (result_.included.insert(rel).second)
            queue_.push_back(rel);
    }

    // `token` is "rom/..." (any case / slashes). Exact file, then a folder, then the start
    // of file names, cutting trailing words off at spaces. True when something matched.
    bool ResolveAndAdd(const std::string& rawToken) {
        std::string rel = NormalizeText(rawToken);
        if (!StartsWith(rel, "rom/"))
            return false;
        rel.erase(0, 4);
        for (;;) {
            TrimEnd(rel, " ./");
            if (rel.empty())
                return false;
            const auto exact = lowerToActual_.find(rel);
            if (exact != lowerToActual_.end()) {
                Add(exact->second);
                return true;
            }
            if (AddRange(rel + "/"))
                return true;
            if (rel.size() >= 4 && AddRange(rel))
                return true;
            const std::size_t space = rel.find_last_of(' ');
            if (space == std::string::npos)
                return false;
            rel.erase(space);
        }
    }

    bool AddRange(const std::string& prefix) {
        bool any = false;
        for (auto it = lowerToActual_.lower_bound(prefix);
             it != lowerToActual_.end() && StartsWith(it->first, prefix); ++it) {
            Add(it->second);
            any = true;
        }
        return any;
    }

    void FollowTokens(const std::string& text, bool strictStart) {
        for (const std::string& token : CollectRomTokens(text, strictStart))
            ResolveAndAdd(token);
    }

    // Scene files named in JSON or code ("scene_next.json").
    void FollowSceneNames(const std::string& text) {
        const std::string lower = Lower(text);
        for (const SceneName& scene : sceneNames_) {
            if (lower.find(scene.baseName) != std::string::npos ||
                (scene.fromSceneDir != scene.baseName && lower.find(scene.fromSceneDir) != std::string::npos))
                Add(scene.rel);
        }
    }

    // Texture / model / effect file names stored inside FBX, OBJ, MTL and Effekseer files.
    void FollowAssetNames(const std::string& text) {
        static const char* kExts[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds", ".efkmodel",
                                      ".efkmat", ".efkefc", ".efk", ".fbx", ".wav", ".ogg", ".mp3"};
        const std::string lower = Lower(text);
        for (const char* ext : kExts) {
            const std::string needle = ext;
            for (std::size_t pos = lower.find(needle); pos != std::string::npos;
                 pos = lower.find(needle, pos + 1)) {
                const std::size_t end = pos + needle.size();
                if (end < lower.size() && std::isalnum(static_cast<unsigned char>(lower[end])))
                    continue;
                std::size_t start = pos;
                while (start > 0) {
                    const char c = lower[start - 1];
                    if (c == '\n' || c == '/' || c == '\\' || c == '"' || c == '|' || c == '<' ||
                        c == '>' || c == '*' || c == '?' || c == ':' || c == '\0')
                        break;
                    --start;
                }
                const std::string name = lower.substr(start, end - start);
                auto tryName = [&](const std::string& candidate) {
                    const auto hit = byBaseName_.find(candidate);
                    if (hit == byBaseName_.end())
                        return false;
                    for (const std::string& rel : hit->second)
                        Add(rel);
                    return true;
                };
                if (tryName(name))
                    continue;
                // "map_Kd tex.png": drop leading words until the name matches a file.
                std::string rest = name;
                bool found = false;
                for (std::size_t space = rest.find(' '); !found && space != std::string::npos;
                     space = rest.find(' ')) {
                    rest.erase(0, space + 1);
                    found = tryName(rest);
                }
                // A length prefix or a neighbouring string can leave a stray character in
                // front of a name pulled out of a binary ("g" + "hero_normal.png").
                for (std::size_t drop = 1; !found && drop <= 4 && drop < name.size(); ++drop)
                    found = tryName(name.substr(drop));
            }
        }
    }

    void ScanRomFile(const std::string& rel) {
        const std::string ext = Extension(rel);
        const bool text = ext == ".json" || ext == ".txt" || ext == ".prefab" || ext == ".physmat" ||
                          ext == ".mtl" || ext == ".obj";
        const bool container = ext == ".fbx" || ext == ".efk" || ext == ".efkefc" || ext == ".efkproj" ||
                               ext == ".efkmodel" || ext == ".efkmat" || ext == ".efkpkg";
        if (!text && !container)
            return;
        std::string bytes;
        if (!ReadAll(options_.romRoot / PathFromUtf8(rel), bytes))
            return;
        const std::string blob = text ? bytes : ExtractStrings(bytes);
        FollowTokens(blob, text);
        if (ext == ".json" || ext == ".txt" || ext == ".prefab" || ext == ".physmat")
            FollowSceneNames(blob);
        if (ext == ".mtl" || ext == ".obj" || container)
            FollowAssetNames(blob);
    }

    void ScanOutsideFile(const fs::path& file) {
        std::string bytes;
        if (!ReadAll(file, bytes))
            return;
        const std::string ext = Lower(PathToUtf8(file.extension()));
        const bool text = ext == ".json" || ext == ".txt";
        const std::string blob = text ? bytes : ExtractStrings(bytes);
        FollowTokens(blob, text);
        // project.json names lastOpenedScene (editor state); only code loads scenes by name.
        if (!text)
            FollowSceneNames(blob);
    }

    struct SceneName {
        std::string fromSceneDir;  // "sub/scene_a.json"
        std::string baseName;      // "scene_a.json"
        std::string rel;           // actual case, "scene/sub/Scene_A.json"
    };

    const Options& options_;
    Result& result_;
    std::map<std::string, std::string> lowerToActual_;
    std::map<std::string, std::vector<std::string>> byBaseName_;
    std::vector<SceneName> sceneNames_;
    std::deque<std::string> queue_;
};

}  // namespace

std::string ExtractStrings(const std::string& bytes) {
    std::string out;

    std::string run;
    auto flush = [&]() {
        if (run.size() >= 5) {
            out += run;
            out.push_back('\n');
        }
        run.clear();
    };
    for (unsigned char c : bytes) {
        if ((c >= 0x20 && c < 0x7F) || c >= 0x80)
            run.push_back(static_cast<char>(c));
        else
            flush();
    }
    flush();

    // UTF-16LE, ASCII characters only (high byte 0). Accepting other characters too would glue
    // misaligned ASCII text into long runs and hide the "rom/" boundary. A non-ASCII file name
    // in a UTF-16 script string therefore needs build.alwaysInclude; UTF-8 literals are covered above.
    for (std::size_t align = 0; align < 2; ++align) {
        std::string ascii;
        for (std::size_t i = align; i + 1 < bytes.size(); i += 2) {
            const unsigned char lo = static_cast<unsigned char>(bytes[i]);
            const unsigned char hi = static_cast<unsigned char>(bytes[i + 1]);
            if (hi == 0 && lo >= 0x20 && lo < 0x7F) {
                ascii.push_back(static_cast<char>(lo));
            } else {
                if (ascii.size() >= 5) {
                    out += ascii;
                    out.push_back('\n');
                }
                ascii.clear();
            }
        }
        if (ascii.size() >= 5) {
            out += ascii;
            out.push_back('\n');
        }
    }
    return out;
}

std::vector<std::string> CollectRomTokens(const std::string& text, bool strictStart) {
    std::vector<std::string> tokens;
    const std::string norm = NormalizeText(text);
    for (std::size_t pos = norm.find("rom/"); pos != std::string::npos; pos = norm.find("rom/", pos)) {
        if (strictStart && pos > 0) {
            const unsigned char prev = static_cast<unsigned char>(norm[pos - 1]);
            if (std::isalnum(prev) || prev == '_' || prev == '/' || prev == '.') {
                pos += 4;
                continue;
            }
        }
        std::size_t end = pos + 4;
        while (end < norm.size() && !IsTerminator(static_cast<unsigned char>(norm[end])))
            ++end;
        std::string token = norm.substr(pos, end - pos);
        TrimEnd(token, " .");
        if (token.size() > 4)
            tokens.push_back(std::move(token));
        pos = end;
    }
    return tokens;
}

Result ComputeReferencedAssets(const Options& options) {
    Result result;
    Scanner(options, result).Run();
    return result;
}

}  // namespace package_assets
