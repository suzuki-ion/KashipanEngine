#pragma once
#include <string>
#include "EngineSettings.h"
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {

// limits セクションの読込
inline void LoadLimitsSettings(const JSON &rootJSON, EngineSettings &settings) {
    JSON limitsJSON = rootJSON.value("limits", JSON::object());
    settings.limits.maxTextures = limitsJSON.value("maxTextures", settings.limits.maxTextures);
    settings.limits.maxSounds = limitsJSON.value("maxSounds", settings.limits.maxSounds);
    settings.limits.maxModels = limitsJSON.value("maxModels", settings.limits.maxModels);
    settings.limits.maxGameObjects = limitsJSON.value("maxGameObjects", settings.limits.maxGameObjects);
    settings.limits.maxComponentsPerGameObject = limitsJSON.value("maxComponentsPerGameObject", settings.limits.maxComponentsPerGameObject);
    settings.limits.maxWindows = limitsJSON.value("maxWindows", settings.limits.maxWindows);

    LogSeparator();
    Log("Limits", LogSeverity::Info);
    LogSeparator();
    Log("Max Textures: " + std::to_string(settings.limits.maxTextures), LogSeverity::Info);
    Log("Max Sounds: " + std::to_string(settings.limits.maxSounds), LogSeverity::Info);
    Log("Max Models: " + std::to_string(settings.limits.maxModels), LogSeverity::Info);
    Log("Max Game Objects: " + std::to_string(settings.limits.maxGameObjects), LogSeverity::Info);
    Log("Max Components Per Game Object: " + std::to_string(settings.limits.maxComponentsPerGameObject), LogSeverity::Info);
    Log("Max Windows: " + std::to_string(settings.limits.maxWindows), LogSeverity::Info);
}

} // namespace KashipanEngine
