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
}

} // namespace KashipanEngine
