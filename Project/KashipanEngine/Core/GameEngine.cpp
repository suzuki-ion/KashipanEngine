#include "GameEngine.h"
#include "EngineSettings.h"
#include "Core/Window.h"
#include "Graphics/Renderer.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/Translation.h"
#include "Utilities/TimeUtils.h"
#include "Objects/GameObjects/3D/Model.h"
#include "Graphics/ScreenBuffer.h"
#include "Graphics/ShadowMapBuffer.h"
#include "AppInitialize.h"

#include "Scene/SceneBase.h"

#include <cstdint>
#include <algorithm>
#include <chrono>

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

namespace KashipanEngine {
namespace {
/// @brief エンジン初期化フラグ
bool sIsEngineInitialized = false;
} // namespace

#if defined(USE_IMGUI)
GameEngine::RollingAverage::RollingAverage(std::size_t capacity) {
    SetCapacity(capacity);
}

void GameEngine::RollingAverage::SetCapacity(std::size_t capacity) {
    capacity = std::max<std::size_t>(1, capacity);
    if (capacity_ == capacity) return;

    capacity_ = capacity;
    samples_.clear();
    samples_.reserve(capacity_);
    writeIndex_ = 0;
    sum_ = 0.0;
}

void GameEngine::RollingAverage::Add(double value) {
    if (samples_.size() < capacity_) {
        samples_.push_back(value);
        sum_ += value;
        return;
    }

    sum_ -= samples_[writeIndex_];
    samples_[writeIndex_] = value;
    sum_ += value;

    writeIndex_ = (writeIndex_ + 1) % capacity_;
}

double GameEngine::RollingAverage::GetAverage() const {
    if (samples_.empty()) return 0.0;
    return sum_ / static_cast<double>(samples_.size());
}

std::size_t GameEngine::RollingAverage::GetCount() const {
    return samples_.size();
}

void GameEngine::DrawProfilingImGui() {
    const std::size_t cap = static_cast<std::size_t>(std::max(1, profilingSampleCount_));
    avgUpdateMs_.SetCapacity(cap);
    avgDrawMs_.SetCapacity(cap);
    avgFps_.SetCapacity(cap);

    avgUpdateMs_.Add(static_cast<double>(updateMs_));
    avgDrawMs_.Add(static_cast<double>(drawMs_));
    avgFps_.Add(static_cast<double>(fps_));

    if (ImGui::Begin("GameEngine Profiling")) {
        ImGui::Text("FPS: %.2f", fps_);
        ImGui::Text("Update: %.3f ms", updateMs_);
        ImGui::Text("Draw: %.3f ms", drawMs_);
        ImGui::Separator();
        ImGui::SliderInt("Profiling Sample Count", &profilingSampleCount_, 1, 600);
        ImGui::Text("Averages (%zu samples)", avgUpdateMs_.GetCount());
        ImGui::Text("Avg FPS: %.2f", static_cast<float>(avgFps_.GetAverage()));
        ImGui::Text("Avg Update: %.3f ms", static_cast<float>(avgUpdateMs_.GetAverage()));
        ImGui::Text("Avg Draw: %.3f ms", static_cast<float>(avgDrawMs_.GetAverage()));
    }
    ImGui::End();
}
#endif

GameEngine::GameEngine(PasskeyForGameEngineMain) {
    LogScope scope;
    LogSeparator();
    Log(Translation("engine.initialize.start"));
    LogSeparator();

    if (sIsEngineInitialized) {
        throw std::runtime_error("GameEngine instance already exists.");
    }
    sIsEngineInitialized = true;

    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to initialize COM library.");
    }

    //--------- インスタンス生成 ---------//

    sceneManager_ = std::make_unique<SceneManager>(Passkey<GameEngine>());

    windowsAPI_ = std::make_unique<WindowsAPI>(Passkey<GameEngine>{});
    directXCommon_ = std::make_unique<DirectXCommon>(Passkey<GameEngine>{});
    ScreenBuffer::SetDirectXCommon(Passkey<GameEngine>{}, directXCommon_.get());
    ShadowMapBuffer::SetDirectXCommon(Passkey<GameEngine>{}, directXCommon_.get());
    graphicsEngine_ = std::make_unique<GraphicsEngine>(Passkey<GameEngine>{}, directXCommon_.get());

    if (graphicsEngine_) {
        auto* renderer = graphicsEngine_->GetRenderer(Passkey<GameEngine>{});
        ScreenBuffer::SetRenderer(Passkey<GameEngine>{}, renderer);
        Object2DBase::SetRenderer(Passkey<GameEngine>{}, renderer);
        Object3DBase::SetRenderer(Passkey<GameEngine>{}, renderer);
    }

    textureManager_ = std::make_unique<TextureManager>(Passkey<GameEngine>{}, directXCommon_.get(), "Assets");
    samplerManager_ = std::make_unique<SamplerManager>(Passkey<GameEngine>{}, directXCommon_.get());
    modelManager_ = std::make_unique<ModelManager>(Passkey<GameEngine>{}, "Assets");
    audioManager_ = std::make_unique<AudioManager>(Passkey<GameEngine>{}, "Assets");
    Model::SetModelManager(Passkey<GameEngine>{}, modelManager_.get());
#if defined(USE_IMGUI)
    imguiManager_ = std::make_unique<ImGuiManager>(Passkey<GameEngine>{}, windowsAPI_.get(), directXCommon_.get());
#endif
    input_ = std::make_unique<Input>(Passkey<GameEngine>{});
    inputCommand_ = std::make_unique<InputCommand>(Passkey<GameEngine>{}, input_.get());

    context_.engine = this;
    context_.sceneManager = sceneManager_.get();
    context_.inputCommand = inputCommand_.get();

    SceneBase::SetEnginePointers(
        Passkey<GameEngine>{},
        audioManager_.get(),
        modelManager_.get(),
        samplerManager_.get(),
        textureManager_.get(),
        input_.get(),
        inputCommand_.get());

    const auto &windowSettings = GetEngineSettings().window;
    Window::SetDefaultParams({},
        windowSettings.initialWindowTitle,
        windowSettings.initialWindowWidth,
        windowSettings.initialWindowHeight,
        windowSettings.initialWindowStyle,
        windowSettings.initialWindowIconPath);

    Window::SetWindowsAPI({}, windowsAPI_.get());
    Window::SetDirectXCommon({}, directXCommon_.get());

    //--------- ゲームループ終了条件 ---------//

    gameLoopEndConditionFunction_ = []() {
        return Window::GetWindowCount() == 0;
    };

    AppInitialize(context_);

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
    ScreenBuffer::AllDestroy({});
    ShadowMapBuffer::AllDestroy({});

    inputCommand_.reset();
    input_.reset();

#if defined(USE_IMGUI)
    imguiManager_.reset();
#endif

    audioManager_.reset();
    modelManager_.reset();
    samplerManager_.reset();
    textureManager_.reset();

    ScreenBuffer::SetRenderer(Passkey<GameEngine>{}, nullptr);
    Object2DBase::SetRenderer(Passkey<GameEngine>{}, nullptr);
    Object3DBase::SetRenderer(Passkey<GameEngine>{}, nullptr);

    graphicsEngine_.reset();
    directXCommon_.reset();
    windowsAPI_.reset();
    sIsEngineInitialized = false;

    CoUninitialize();

    LogSeparator();
    Log(Translation("engine.finalize.end"));
    LogSeparator();
}

void GameEngine::GameLoopUpdate() {
#if defined(USE_IMGUI)
    const auto beginTp = std::chrono::high_resolution_clock::now();
#endif

    Window::Update({});
    UpdateDeltaTime({});

    if (input_) {
        input_->Update();
    }

#if defined(USE_IMGUI)
    {
        const float dt = GetDeltaTime();
        fps_ = (dt > 0.0f) ? (1.0f / dt) : 0.0f;
    }
#endif

    if (audioManager_) {
        audioManager_->Update();
    }
#if defined(USE_IMGUI)
    imguiManager_->BeginFrame({});
#endif

    if (sceneManager_) {
        if (auto *scene = sceneManager_->GetCurrentScene()) {
            if (!isGameLoopPaused_ || isNextFrameRequested_) {
                scene->Update();
                if (isNextFrameRequested_) {
                    isNextFrameRequested_ = false;
                }
            }
#if defined(USE_IMGUI)
            scene->ShowImGui();
#endif
        }
    }

    if (!isGameLoopRunning_ || isGameLoopPaused_) {
#if defined(USE_IMGUI)
        const auto endTp = std::chrono::high_resolution_clock::now();
        updateMs_ = std::chrono::duration<float, std::milli>(endTp - beginTp).count();
#endif
        return;
    }

#if defined(USE_IMGUI)
    const auto endTp = std::chrono::high_resolution_clock::now();
    updateMs_ = std::chrono::duration<float, std::milli>(endTp - beginTp).count();
#endif
}

void GameEngine::GameLoopDraw() {
#if defined(USE_IMGUI)
    const auto beginTp = std::chrono::high_resolution_clock::now();
#endif

    directXCommon_->BeginDraw({});
    Window::Draw({});

    graphicsEngine_->RenderFrame({});

#if defined(USE_IMGUI)
    if (imguiManager_) {
        DrawProfilingImGui();

        if (graphicsEngine_) {
            if (auto *renderer = graphicsEngine_->GetRenderer({})) {
                renderer->ShowImGuiCpuTimersWindow();
            }
        }

        ScreenBuffer::ShowImGuiScreenBuffersWindow();
        ShadowMapBuffer::ShowImGuiShadowMapBuffersWindow();

        ImGui::Begin("GameLoop Control");
        if (isGameLoopPaused_) {
            if (ImGui::Button("Resume Game Loop")) {
                GameLoopResume();
            }
            if (ImGui::Button("Step Frame")) {
                isNextFrameRequested_ = true;
            }
        } else {
            if (ImGui::Button("Pause Game Loop")) {
                GameLoopPause();
            }
        }
        ImGui::End();

        imguiManager_->Render({});
    }
#endif

    directXCommon_->EndDraw({});

#if defined(USE_IMGUI)
    // 描画時間の計測だけ最後に行う（ImGuiやDirectXCommonのEndDrawも含めるため）
    const auto endTp = std::chrono::high_resolution_clock::now();
    drawMs_ = std::chrono::duration<float, std::milli>(endTp - beginTp).count();
#endif
}

int GameEngine::Execute(PasskeyForGameEngineMain) {
    while (!gameLoopEndConditionFunction_()) {
        GameLoopUpdate();
        GameLoopDraw();

        if (sceneManager_) {
            sceneManager_->CommitPendingSceneChange({});
        }
        Window::CommitDestroy({});
        ScreenBuffer::CommitDestroy({});
        ShadowMapBuffer::CommitDestroy({});
        directXCommon_->AllDestroyPendingSwapChains({});
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