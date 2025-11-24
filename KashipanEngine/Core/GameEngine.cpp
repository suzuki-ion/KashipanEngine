#include "GameEngine.h"
#include "EngineSettings.h"
#include "Core/Window.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/Translation.h"
#include "Utilities/TimeUtils.h"
#include "Core/WindowsAPI/WindowEvents/DefaultEvents.h"

namespace KashipanEngine {
namespace {
/// @brief エンジン初期化フラグ
bool sIsEngineInitialized = false;
} // namespace

GameEngine::GameEngine(PasskeyForGameEngineMain) {
    LogScope scope;
    LogSeparator();
    Log(Translation("engine.initialize.start"));
    LogSeparator();

    if (sIsEngineInitialized) {
        throw std::runtime_error("GameEngine instance already exists.");
    }
    sIsEngineInitialized = true;

    //--------- インスタンス生成 ---------//

    windowsAPI_ = std::make_unique<WindowsAPI>(Passkey<GameEngine>{});
    directXCommon_ = std::make_unique<DirectXCommon>(Passkey<GameEngine>{});
    graphicsEngine_ = std::make_unique<GraphicsEngine>(Passkey<GameEngine>{}, directXCommon_.get());

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

    auto monitorInfo = windowsAPI_->QueryMonitorInfo();
    Window *parentWindow = Window::CreateOverlay("Overlay Window", monitorInfo->WorkArea().right, monitorInfo->WorkArea().bottom, true);
    parentWindow->RegisterWindowEvent(std::make_unique<WindowDefaultEvent::SysCommandCloseEventSimple>());
    Window *mainWindow = nullptr;
    mainWindow = Window::CreateNormal();
    mainWindow->SetWindowParent(parentWindow, false);
    Window *window = nullptr;
    for (int i = 0; i < 8; ++i) {
        window = Window::CreateNormal(std::string("Sub Window ") + std::to_string(i + 1), 512, 128);
        window->RegisterWindowEvent(std::make_unique<WindowDefaultEvent::SysCommandCloseEventSimple>());
        window->SetWindowParent(parentWindow, false);
    }

    //--------- ゲームループ終了条件 ---------//

    gameLoopEndConditionFunction_ = []() {
        return Window::GetWindowCount() == 0;
    };

    LogSeparator();
    Log(Translation("engine.initialize.end"));
    LogSeparator();
}

GameEngine::~GameEngine() {
    LogScope scope;
    LogSeparator();
    Log(Translation("engine.finalize.start"));
    LogSeparator();

    Window::AllDestroy({});
    directXCommon_.reset();
    windowsAPI_.reset();
    sIsEngineInitialized = false;

    LogSeparator();
    Log(Translation("engine.finalize.end"));
    LogSeparator();
}

void GameEngine::GameLoopUpdate() {
    Window::Update({});
    UpdateDeltaTime({});
    if (!isGameLoopRunning_ || isGameLoopPaused_) {
        return;
    }
}

void GameEngine::GameLoopDraw() {
    directXCommon_->BeginDraw({});
    Window::Draw({});
    graphicsEngine_->RenderFrame({});
    directXCommon_->EndDraw({});
}

int GameEngine::Execute(PasskeyForGameEngineMain) {
    while (!gameLoopEndConditionFunction_()) {
        GameLoopUpdate();
        GameLoopDraw();
        Window::CommitDestroy({});
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