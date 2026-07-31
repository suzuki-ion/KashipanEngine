#pragma once
#include <string>
#include "EngineSettings.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/Translation.h"

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
    Log(Translation("engine.settings.limits.section"), LogSeverity::Info);
    LogSeparator();
    Log(Translation("engine.settings.limits.maxtextures") + std::to_string(settings.limits.maxTextures), LogSeverity::Info);
    Log(Translation("engine.settings.limits.maxsounds") + std::to_string(settings.limits.maxSounds), LogSeverity::Info);
    Log(Translation("engine.settings.limits.maxmodels") + std::to_string(settings.limits.maxModels), LogSeverity::Info);
    Log(Translation("engine.settings.limits.maxgameobjects") + std::to_string(settings.limits.maxGameObjects), LogSeverity::Info);
    Log(Translation("engine.settings.limits.maxcomponentspergameobject") + std::to_string(settings.limits.maxComponentsPerGameObject), LogSeverity::Info);
    Log(Translation("engine.settings.limits.maxwindows") + std::to_string(settings.limits.maxWindows), LogSeverity::Info);
}

} // namespace KashipanEngine
