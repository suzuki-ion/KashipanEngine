#pragma once
#include <string>
#include "EngineSettings.h"
#include "Utilities/FileIO/Json.h"

namespace KashipanEngine {

// rendering セクションの読込
inline void LoadRenderingSettings(const Json &rootJson, EngineSettings &settings) {
    Json renderingJson = rootJson.value("rendering", Json::object());
    auto clearColorJson = renderingJson.value(
        "defaultClearColor",
        Json::array({
            settings.rendering.defaultClearColor[0],
            settings.rendering.defaultClearColor[1],
            settings.rendering.defaultClearColor[2],
            settings.rendering.defaultClearColor[3]
        })
    );

    for (int i = 0; i < 4 && i < static_cast<int>(clearColorJson.size()); ++i) {
        settings.rendering.defaultClearColor[i] = clearColorJson[i];
    }

    settings.rendering.enableVSync = renderingJson.value("enableVSync", settings.rendering.enableVSync);
    settings.rendering.maxFPS = renderingJson.value("maxFPS", settings.rendering.maxFPS);
}

} // namespace KashipanEngine
