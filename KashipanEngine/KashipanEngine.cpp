#include "KashipanEngine.h"
#include "EngineSettings.h"

int KashipanEngine::Execute(PasskeyForWinMain, const std::string &engineSettingsPath) {
    LoadEngineSettings({}, engineSettingsPath);
    std::unique_ptr<GameEngine> engine;
    engine = std::make_unique<GameEngine>(engineSettingsPath);
    return engine->Execute();
}
