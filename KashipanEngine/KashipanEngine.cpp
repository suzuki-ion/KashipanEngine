#include "KashipanEngine.h"
#include "EngineSettings.h"
#include "Debug/LogSettings.h"
#include "Debug/Logger.h"
#include "Utilities/FileIO/Json.h"

namespace KashipanEngine {
int Execute(PasskeyForWinMain, const std::string &engineSettingsPath) {
    //--------- 設定ファイル読み込み ---------//

    Json engineSettingsJson = LoadJson(engineSettingsPath);
    if (engineSettingsJson.is_null()) {
        assert(false && "Failed to load engine settings JSON.");
        return -1;
    }
    std::string logSettingsPath = engineSettingsJson.value("LogSettingsPath", "LogSettings.json");
    LoadLogSettings({}, logSettingsPath);
    InitializeLogger({});
    LoadEngineSettings({}, engineSettingsPath);

    //--------- エンジン実行 ---------//

    std::unique_ptr<GameEngine> engine = std::make_unique<GameEngine>(PasskeyForGameEngineMain{});
    int code = engine->Execute();

    //--------- エンジン終了 ---------//

    engine.reset();
    ShutdownLogger({});
    return code;
}

} // namespace KashipanEngine