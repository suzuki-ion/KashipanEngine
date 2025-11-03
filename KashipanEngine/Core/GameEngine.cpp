#include "GameEngine.h"
#include "EngineSettings.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/Translation.h"
#include "Utilities/TimeUtils.h"

namespace KashipanEngine {
namespace {
/// @brief エンジン初期化フラグ
bool sIsEngineInitialized = false;
} // namespace

GameEngine::GameEngine(PasskeyForGameEngineMain) {
    LogScope scope;
    Log(Translation("engine.initialize.start"));
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
    
    mainWindow_ = Window::Create("Main Window");
    Window::Create("Sub Window");

    Log(Translation("engine.initialize.end"));
}

GameEngine::~GameEngine() {
    LogScope scope;
    Log(Translation("engine.finalize.start"));
    Window::AllDestroy({});
    directXCommon_.reset();
    windowsAPI_.reset();
    sIsEngineInitialized = false;
    Log(Translation("engine.finalize.end"));
}

void GameEngine::GameLoopUpdate() {
    Window::Update({});
    UpdateDeltaTime({});
    if (!isGameLoopRunning_ || isGameLoopPaused_) {
        return;
    }
}

void GameEngine::GameLoopDraw() {
}

int GameEngine::Execute() {
    // メインウィンドウが終了するまでループ
    while (Window::IsExist(mainWindow_)) {
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