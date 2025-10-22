#include "GameEngine.h"
#include "Utilities/FileIO/Json.h"
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

GameEngine::~GameEngine() {}

GameEngine::GameEngine(const std::string &engineSettingsPath) {
    LogScope scope; // 正しい関数名を出すためローカルで生成
    Log("Creating GameEngine instance.");
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

    Log("Created GameEngine instance.");
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