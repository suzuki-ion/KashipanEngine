#include "KashipanEngine.h"
#include "EngineSettings.h"
#include "Debug/LogSettings.h"
#include "Debug/Logger.h"
#include "Utilities/FileIO/Json.h"

namespace KashipanEngine {
int Execute(PasskeyForWinMain, const std::string &engineSettingsPath) {
    Json engineSettingsJson = LoadJson(engineSettingsPath);
    if (engineSettingsJson.is_null()) {
        assert(false && "Failed to load engine settings JSON.");
        return -1;
    }
    std::string logSettingsPath = engineSettingsJson.value("LogSettingsPath", "LogSettings.json");
    LoadLogSettings({}, logSettingsPath);
    InitializeLogger({});
    LoadEngineSettings({}, engineSettingsPath);

    std::unique_ptr<GameEngine> engine;
    engine = std::make_unique<GameEngine>(engineSettingsPath);
    int code = engine->Execute();

    ShutdownLogger({});
    return code;
}

} // namespace KashipanEngine