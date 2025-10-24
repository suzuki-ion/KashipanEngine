#include "GameEngine.h"
#include "Utilities/FileIO/Json.h"
#include "Utilities/Translation.h"
#include "Debug/Logger.h"

namespace KashipanEngine {
namespace {
/// @brief エンジン初期化フラグ
bool sIsEngineInitialized = false;
/// @brief エンジン生成時のウィンドウタイトル
std::string sInitialWindowTitle = "KashipanEngine";
/// @brief エンジン生成時のウィンドウ幅
int32_t sInitialWindowWidth = 1280;
/// @brief エンジン生成時のウィンドウ高さ
int32_t sInitialWindowHeight = 720;
/// @brief エンジン生成時のウィンドウアイコンパス
std::string sInitialWindowIconPath = "";
} // namespace

GameEngine::GameEngine(const std::string &engineSettingsPath) {
    LogScope scope;
    Log(GetTranslationText("engine.log.instance.creating") + "GameEngine");
    if (sIsEngineInitialized) {
        throw std::runtime_error("GameEngine instance already exists.");
    }
    sIsEngineInitialized = true;
    ParseEngineSettings(engineSettingsPath);

    windowsAPI_ = std::make_unique<WindowsAPI>(
        Passkey<GameEngine>{},
        sInitialWindowTitle,
        sInitialWindowWidth,
        sInitialWindowHeight);

    directXCommon_ = std::make_unique<DirectXCommon>(
        Passkey<GameEngine>{});
    
    mainWindow_ = windowsAPI_->CreateWindowInstance(
        sInitialWindowTitle,
        sInitialWindowWidth,
        sInitialWindowHeight);

    Log(GetTranslationText("engine.log.instance.created") + "GameEngine");
}

GameEngine::~GameEngine() {
    LogScope scope;
    Log(GetTranslationText("engine.log.instance.destroying") + "GameEngine");
    if (mainWindow_) {
        windowsAPI_->DestroyWindowInstance(mainWindow_);
        mainWindow_ = nullptr;
    }
    directXCommon_.reset();
    windowsAPI_.reset();
    sIsEngineInitialized = false;
    Log(GetTranslationText("engine.log.instance.destroyed") + "GameEngine");
}

void GameEngine::ParseEngineSettings(const std::string &engineSettingsPath) {
    LogScope scope;
    Log("Parsing engine settings from: " + engineSettingsPath);
    Json engineSettings = LoadJson(engineSettingsPath);

    sInitialWindowTitle = engineSettings.value("InitialWindowTitle", sInitialWindowTitle);
    sInitialWindowWidth = engineSettings.value("InitialWindowWidth", sInitialWindowWidth);
    sInitialWindowHeight = engineSettings.value("InitialWindowHeight", sInitialWindowHeight);
    sInitialWindowIconPath = engineSettings.value("InitialWindowIconPath", sInitialWindowIconPath);
    Log("Parsed engine settings.");
}

void GameEngine::GameLoopUpdate() {
    windowsAPI_->Update({});
    if (!isGameLoopRunning_ || isGameLoopPaused_) {
        return;
    }
}

void GameEngine::GameLoopDraw() {
}

int GameEngine::Execute() {
    // メインウィンドウが終了するまでループ
    while (!mainWindow_->IsDestroyed()) {
        GameLoopUpdate();
        GameLoopDraw();
    }
    return 0;
}

void GameEngine::GameLoopRun() {
}

void GameEngine::GameLoopEnd() {
}

void GameEngine::GameLoopPause() {
}

} // namespace KashipanEngine