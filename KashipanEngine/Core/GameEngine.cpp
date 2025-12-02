#include "GameEngine.h"
#include "EngineSettings.h"
#include "Core/Window.h"
#include "Core/WindowsAPI/WindowEvents/DefaultEvents.h"
#include "Graphics/Renderer.h"
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
    windows_.emplace_back(Window::CreateOverlay("Overlay Window", monitorInfo->WorkArea().right, monitorInfo->WorkArea().bottom, true));
    windows_.front()->RegisterWindowEvent(std::make_unique<WindowDefaultEvent::SysCommandCloseEventSimple>());
    windows_.emplace_back(Window::CreateNormal("Main Window"));
    windows_.back()->SetWindowParent(windows_.front(), false);
    for (int i = 0; i < 4; ++i) {
        windows_.emplace_back(Window::CreateNormal(std::string("Sub Window ") + std::to_string(i + 1), 512, 128));
        windows_.back()->RegisterWindowEvent(std::make_unique<WindowDefaultEvent::SysCommandCloseEventSimple>());
        windows_.back()->SetWindowParent(windows_.front(), false);
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

    // テスト用更新処理
    // ウィンドウタイトルにFPS表示
    for (auto *window : windows_) {
        if (window && window->IsExist(window) && window->GetWindowTitle() != "Main Window" && window->GetWindowTitle() != "Overlay Window") {
            const float fps = 1.0f / GetDeltaTime();
            window->SetWindowTitle(std::string("KashipanEngine - FPS: ") + std::to_string(static_cast<int>(fps)));
        }
    }
    // Main Window をsinを使って上下に移動させる
    static float t = 0.0f;
    t += GetDeltaTime();
    if (auto *mainWindow = Window::GetWindows("Main Window").front()) {
        auto monitorInfo = windowsAPI_->QueryMonitorInfo();
        const int32_t centerY = (monitorInfo->WorkArea().bottom - monitorInfo->WorkArea().top) / 2;
        const int32_t amplitude = (monitorInfo->WorkArea().bottom - monitorInfo->WorkArea().top) / 4;
        const int32_t newY = centerY + static_cast<int32_t>(amplitude * std::sin(t));
        mainWindow->SetWindowPosition(mainWindow->GetWindowPosition().x, newY);
    }

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