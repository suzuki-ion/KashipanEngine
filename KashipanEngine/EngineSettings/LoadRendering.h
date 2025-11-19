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
    settings.rendering.defaultEnableVSync = renderingJSON.value("defaultEnableVSync", settings.rendering.defaultEnableVSync);
    settings.rendering.defaultMaxFPS = renderingJSON.value("defaultMaxFPS", settings.rendering.defaultMaxFPS);
    settings.rendering.pipelineSettingsPath = renderingJSON.value("pipelineSettingsPath", settings.rendering.pipelineSettingsPath);
    settings.rendering.rtvDescriptorHeapSize = renderingJSON.value("rtvDescriptorHeapSize", settings.rendering.rtvDescriptorHeapSize);
    settings.rendering.dsvDescriptorHeapSize = renderingJSON.value("dsvDescriptorHeapSize", settings.rendering.dsvDescriptorHeapSize);
    settings.rendering.srvDescriptorHeapSize = renderingJSON.value("srvDescriptorHeapSize", settings.rendering.srvDescriptorHeapSize);
}

} // namespace KashipanEngine
