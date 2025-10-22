#include "EngineSettings.h"
#include "Utilities/FileIO/Json.h"

namespace KashipanEngine {
namespace {
EngineSettings sEngineSettings;
} // namespace

const EngineSettings &LoadEngineSettings(PasskeyForGameEngineMain, const std::string &engineSettingsPath) {
    Json json = LoadJson(engineSettingsPath);
    if (json.empty()) {
        return sEngineSettings;
    }
    //--------- ウィンドウのデフォルト設定 ---------//
    Json windowJson = json.value("window", Json::object());
    sEngineSettings.window.initialWindowTitle = windowJson.value("initialWindowTitle", sEngineSettings.window.initialWindowTitle);
    sEngineSettings.window.initialWindowWidth = windowJson.value("initialWindowWidth", sEngineSettings.window.initialWindowWidth);
    sEngineSettings.window.initialWindowHeight = windowJson.value("initialWindowHeight", sEngineSettings.window.initialWindowHeight);
    sEngineSettings.window.initialWindowIconPath = windowJson.value("initialWindowIconPath", sEngineSettings.window.initialWindowIconPath);

    //--------- エンジンリソースの制限設定 ---------//
    Json limitsJson = json.value("limits", Json::object());
    sEngineSettings.limits.maxTextures = limitsJson.value("maxTextures", sEngineSettings.limits.maxTextures);
    sEngineSettings.limits.maxSounds = limitsJson.value("maxSounds", sEngineSettings.limits.maxSounds);
    sEngineSettings.limits.maxModels = limitsJson.value("maxModels", sEngineSettings.limits.maxModels);
    sEngineSettings.limits.maxGameObjects = limitsJson.value("maxGameObjects", sEngineSettings.limits.maxGameObjects);
    sEngineSettings.limits.maxComponentsPerGameObject = limitsJson.value("maxComponentsPerGameObject", sEngineSettings.limits.maxComponentsPerGameObject);

    //--------- レンダリングのデフォルト設定 ---------//
    Json renderingJson = json.value("rendering", Json::object());
    auto clearColorJson = renderingJson.value("defaultClearColor",
        Json::array({
            sEngineSettings.rendering.defaultClearColor[0],
            sEngineSettings.rendering.defaultClearColor[1],
            sEngineSettings.rendering.defaultClearColor[2],
            sEngineSettings.rendering.defaultClearColor[3]
            })
    );
    for (int i = 0; i < 4 && i < clearColorJson.size(); ++i) {
        sEngineSettings.rendering.defaultClearColor[i] = clearColorJson[i];
    }
    sEngineSettings.rendering.enableVSync = renderingJson.value("enableVSync", sEngineSettings.rendering.enableVSync);
    sEngineSettings.rendering.maxFPS = renderingJson.value("maxFPS", sEngineSettings.rendering.maxFPS);

    return sEngineSettings;
}

const EngineSettings &GetEngineSettings() {
    return sEngineSettings;
}

} // namespace KashipanEngine