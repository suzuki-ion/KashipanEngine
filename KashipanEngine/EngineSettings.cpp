#include "EngineSettings.h"
#include "Debug/Logger.h"
#include "Utilities/FileIO/Json.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {
namespace {
EngineSettings sEngineSettings;
} // namespace

const EngineSettings &LoadEngineSettings(PasskeyForGameEngineMain, const std::string &engineSettingsPath) {
    LogScope scope;
    Json json = LoadJson(engineSettingsPath);
    if (json.empty()) {
        Log("Failed engine settings file: " + engineSettingsPath, LogSeverity::Warning);
        Log("Using default engine settings.", LogSeverity::Info);
        return sEngineSettings;
    }
    //--------- エンジンの翻訳ファイル設定 ---------//
    Json translationsJson = json.value("translations", Json::object());
    for (auto &[lang, pathJson] : translationsJson.items()) {
        sEngineSettings.translations.languageFilePaths[lang] = pathJson.get<std::string>();
        LoadTranslationFile(pathJson.get<std::string>());
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

    Log(GetTranslationText("engine.settings.load.success") + ": " + engineSettingsPath, LogSeverity::Info);
    return sEngineSettings;
}

const EngineSettings &GetEngineSettings() {
    LogScope scope;
    return sEngineSettings;
}

} // namespace KashipanEngine