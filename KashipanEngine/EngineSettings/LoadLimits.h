#pragma once
#include <string>
#include "EngineSettings.h"
#include "Utilities/FileIO/Json.h"

namespace KashipanEngine {

// limits セクションの読込
inline void LoadLimitsSettings(const Json &rootJson, EngineSettings &settings) {
    Json limitsJson = rootJson.value("limits", Json::object());
    settings.limits.maxTextures = limitsJson.value("maxTextures", settings.limits.maxTextures);
    settings.limits.maxSounds = limitsJson.value("maxSounds", settings.limits.maxSounds);
    settings.limits.maxModels = limitsJson.value("maxModels", settings.limits.maxModels);
    settings.limits.maxGameObjects = limitsJson.value("maxGameObjects", settings.limits.maxGameObjects);
    settings.limits.maxComponentsPerGameObject = limitsJson.value("maxComponentsPerGameObject", settings.limits.maxComponentsPerGameObject);
}

} // namespace KashipanEngine
