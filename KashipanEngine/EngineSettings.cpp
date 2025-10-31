#include "EngineSettings.h"
#include "Utilities/FileIO/Json.h"

#include "EngineSettings/LoadTranslations.h"
#include "EngineSettings/LoadWindow.h"
#include "EngineSettings/LoadLimits.h"
#include "EngineSettings/LoadRendering.h"

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
    LoadTranslationsSettings(json, sEngineSettings);

    //--------- ウィンドウのデフォルト設定 ---------//
    LoadWindowSettings(json, sEngineSettings);

    //--------- エンジンリソースの制限設定 ---------//
    LoadLimitsSettings(json, sEngineSettings);

    //--------- レンダリングのデフォルト設定 ---------//
    LoadRenderingSettings(json, sEngineSettings);

    Log(Translation("engine.settings.load.success") + engineSettingsPath, LogSeverity::Info);
    return sEngineSettings;
}

const EngineSettings &GetEngineSettings() {
    LogScope scope;
    return sEngineSettings;
}

} // namespace KashipanEngine