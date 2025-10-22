#pragma once
#include <string>
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

struct EngineSettings {
    //--------- ウィンドウのデフォルト設定 ---------//
    struct Window {
        std::string initialWindowTitle = "KashipanEngine";
        int32_t initialWindowWidth = 1280;
        int32_t initialWindowHeight = 720;
        std::string initialWindowIconPath = "";
    };
    //--------- エンジンリソースの制限設定 ---------//
    struct Limits {
        size_t maxTextures = 2048;
        size_t maxSounds = 512;
        size_t maxModels = 1024;
        size_t maxGameObjects = 1024;
        size_t maxComponentsPerGameObject = 32;
    };
    //--------- レンダリングのデフォルト設定 ---------//
    struct Rendering {
        float defaultClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
        bool enableVSync = true;
        int32_t maxFPS = 60;
    };

    /// @brief コンストラクタ
    /// @param json エンジン設定JSONデータ
    Window window;
    Limits limits;
    Rendering rendering;
};

const EngineSettings &LoadEngineSettings(PasskeyForGameEngineMain, const std::string &engineSettingsPath = "EngineSettings.json");
const EngineSettings &GetEngineSettings();

} // namespace KashipanEngine