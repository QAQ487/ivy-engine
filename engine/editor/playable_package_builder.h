#pragma once

#include <string>

// Builds a playable package of the open content project from the editor GUI.
namespace PlayablePackage {

enum class Configuration {
    Debug = 0,
    Release = 1,
};

const char* ConfigurationName(Configuration configuration);
bool TryParseConfiguration(const std::string& name, Configuration* out);

std::string DefaultDestination(const std::string& contentRoot);

struct Prefs {
    std::string destination;
    Configuration configuration = Configuration::Debug;
};

void LoadPrefs(const std::string& contentRoot, Prefs* out);
void SavePrefs(const std::string& contentRoot, const Prefs& prefs);

// Empty string means the destination is allowed.
std::string DestinationRefusalReason(const std::string& contentRoot,
                                     const std::string& destination);

struct Request {
    std::string contentRoot;
    std::string destination;
    Configuration configuration = Configuration::Debug;
};

bool IsBuildRunning();
bool StartBuild(const Request& request);
void CopyLog(std::string* out);
bool IsBuildFinished();
bool LastBuildSucceeded();
std::string LastBuildResultMessage();

}  // namespace PlayablePackage
