#pragma once

#include <string>
#include <vector>

struct HWND__;
typedef HWND__* HWND;

// Unity-style project root: folder with project.json, rom/, and game/scripts/.
class ProjectManager {
public:
    static constexpr int kSchemaVersion = 1;
    static constexpr const char* kProjectFileName = "project.json";

    struct EditorSettings {
        int uiZoomPercent = 100;
    };

    struct ProjectData {
        std::string name;
        int version = kSchemaVersion;
        std::string lastOpenedScene;
        std::string defaultScene;
        EditorSettings editor;
    };

    static ProjectManager& Instance();

    bool IsOpen() const { return m_open; }
    const std::string& GetRootPath() const { return m_rootPath; }
    const std::string& GetName() const { return m_data.name; }
    const ProjectData& GetData() const { return m_data; }

    // Resolve startup project: recent list, then cwd with rom/, then exe dir with rom/.
    bool BootstrapAtStartup();

    bool OpenProject(const std::string& rootPath, std::string* errorOut = nullptr);
    bool CreateProject(const std::string& parentDir, const std::string& projectName,
                       std::string* errorOut = nullptr);
    // Writes project.json from current in-memory state (does not save the scene file).
    bool SaveProjectFile(std::string* errorOut = nullptr);

    void SetLastOpenedScene(const std::string& fileName);
    void SetDefaultScene(const std::string& fileName);
    void SetEditorUiZoomPercent(int percent);
    int GetEditorUiZoomPercent() const { return m_data.editor.uiZoomPercent; }

    // Absolute path or rom/scene-relative filename suitable for EngineGUI_LoadFromFile.
    std::string ResolveSceneToLoad() const;

    const std::vector<std::string>& GetRecentProjects() const { return m_recent; }
    void TouchRecent(const std::string& rootPath);

    void UpdateWindowTitle(HWND hwnd) const;

    static bool PickFolder(HWND owner, const wchar_t* title, std::string* outPath);
    static std::string GetExecutableDirectory();

private:
    ProjectManager() = default;

    bool ActivateRoot(const std::string& rootPath, std::string* errorOut);
    bool LoadProjectFile(const std::string& rootPath, ProjectData* out, std::string* errorOut);
    bool WriteProjectFile(const std::string& rootPath, const ProjectData& data,
                          std::string* errorOut) const;
    ProjectData MakeDefaultData(const std::string& rootPath, const std::string& name) const;
    void EnsureEngineAssets(const std::string& rootPath) const;
    bool ScaffoldNewProjectTree(const std::string& rootPath, const std::string& projectName,
                                std::string* errorOut);
    void LoadRecentList();
    void SaveRecentList() const;
    static std::string RecentListPath();
    static std::string Utf8FromWide(const std::wstring& wide);
    static std::wstring WideFromUtf8(const std::string& utf8);

    bool m_open = false;
    std::string m_rootPath;
    ProjectData m_data;
    std::vector<std::string> m_recent;
};
