#pragma once
#include <string>
#include "EngineSettings.h"
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {

// rendering セクションの読込
inline void LoadRenderingSettings(const JSON &rootJSON, EngineSettings &settings) {
    JSON renderingJSON = rootJSON.value("rendering", JSON::object());
    auto clearColorJSON = renderingJSON.value(
        "defaultClearColor",
        JSON::array({
            settings.rendering.defaultClearColor[0],
            settings.rendering.defaultClearColor[1],
            settings.rendering.defaultClearColor[2],
            settings.rendering.defaultClearColor[3]
        })
    );

    for (int i = 0; i < 4 && i < static_cast<int>(clearColorJSON.size()); ++i) {
        settings.rendering.defaultClearColor[i] = clearColorJSON[i];
    }

    settings.rendering.enableVSync = renderingJSON.value("enableVSync", settings.rendering.enableVSync);
    settings.rendering.maxFPS = renderingJSON.value("maxFPS", settings.rendering.maxFPS);
}

} // namespace KashipanEngine
