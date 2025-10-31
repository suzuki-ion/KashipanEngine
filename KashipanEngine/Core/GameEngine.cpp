#include "GameEngine.h"
#include "EngineSettings.h"
#include "Utilities/FileIO/Json.h"
#include "Utilities/Translation.h"
#include "Debug/Logger.h"

namespace KashipanEngine {
namespace {
/// @brief エンジン初期化フラグ
bool sIsEngineInitialized = false;
} // namespace

GameEngine::GameEngine(PasskeyForGameEngineMain) {
    LogScope scope;
    Log(Translation("engine.log.instance.creating") + "GameEngine");
    if (sIsEngineInitialized) {
        throw std::runtime_error("GameEngine instance already exists.");
    }
    sIsEngineInitialized = true;

    //--------- インスタンス生成 ---------//

    windowsAPI_ = std::make_unique<WindowsAPI>(Passkey<GameEngine>{});
    directXCommon_ = std::make_unique<DirectXCommon>(Passkey<GameEngine>{});

    //--------- ウィンドウ作成 ---------//
    
    const auto &windowSettings = GetEngineSettings().window;
    Window::SetDefaultParams({},
        windowSettings.initialWindowTitle,
        windowSettings.initialWindowWidth,
        windowSettings.initialWindowHeight,
        windowSettings.initialWindowStyle,
        windowSettings.initialWindowIconPath
    );
    Window::SetWindowsAPI({}, windowsAPI_.get());
    Window::SetDirectXCommon({}, directXCommon_.get());
    
    mainWindowHandle_ = Window::Create("Main Window")->GetWindowHandle();
    Window::Create("Sub Window");

    Log(Translation("engine.log.instance.created") + "GameEngine");
}

GameEngine::~GameEngine() {
    LogScope scope;
    Log(Translation("engine.log.instance.destroying") + "GameEngine");
    Window::AllDestroy({});
    directXCommon_.reset();
    windowsAPI_.reset();
    sIsEngineInitialized = false;
    Log(Translation("engine.log.instance.destroyed") + "GameEngine");
}

void GameEngine::GameLoopUpdate() {
    Window::Update({});
    if (!isGameLoopRunning_ || isGameLoopPaused_) {
        return;
    }
}

void GameEngine::GameLoopDraw() {
}

int GameEngine::Execute() {
    // メインウィンドウが終了するまでループ
    while (Window::IsExist(mainWindowHandle_)) {
        GameLoopUpdate();
        GameLoopDraw();
    }
    return 0;
}

void GameEngine::GameLoopRun() {
    isGameLoopRunning_ = true;
    isGameLoopPaused_ = false;
}

void GameEngine::GameLoopEnd() {
    isGameLoopRunning_ = false;
    isGameLoopPaused_ = false;
}

void GameEngine::GameLoopPause() {
    isGameLoopPaused_ = true;
}

void GameEngine::GameLoopResume() {
    isGameLoopPaused_ = false;
}

} // namespace KashipanEngine