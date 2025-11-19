#pragma once
#include <unordered_map>

namespace KashipanEngine {

struct EngineSettings {
    //--------- ウィンドウのデフォルト設定 ---------//
    struct Window {
        std::string initialWindowTitle = "KashipanEngine";
        int32_t initialWindowWidth = 1280;
        int32_t initialWindowHeight = 720;
        DWORD initialWindowStyle = 0;
        std::string initialWindowIconPath = "";
    };
    //--------- エンジンリソースの制限設定 ---------//
    struct Limits {
        size_t maxTextures = 2048;
        size_t maxSounds = 512;
        size_t maxModels = 1024;
        size_t maxGameObjects = 1024;
        size_t maxComponentsPerGameObject = 32;
        size_t maxWindows = 32;
    };
    //--------- レンダリングのデフォルト設定 ---------//
    struct Rendering {
        float defaultClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        bool defaultEnableVSync = true;
        int32_t defaultMaxFPS = 60;
        std::string pipelineSettingsPath = "";
        UINT rtvDescriptorHeapSize = 64;
        UINT dsvDescriptorHeapSize = 64;
        UINT srvDescriptorHeapSize = 512;
    };
    //--------- エンジンの翻訳ファイル設定 ---------//
    struct Translations {
        std::unordered_map<std::string, std::string> languageFilePaths;
    };

    Window window;
    Limits limits;
    Rendering rendering;
    Translations translations;
};

const EngineSettings &LoadEngineSettings(PasskeyForGameEngineMain, const std::string &engineSettingsPath = "EngineSettings.json");
const EngineSettings &GetEngineSettings();

} // namespace KashipanEngine