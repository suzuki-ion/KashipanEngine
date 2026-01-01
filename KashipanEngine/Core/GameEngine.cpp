#include "GameEngine.h"
#include "EngineSettings.h"
#include "Core/Window.h"
#include "Core/WindowsAPI/WindowEvents/DefaultEvents.h"
#include "Graphics/Renderer.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/Translation.h"
#include "Utilities/TimeUtils.h"
#include "Objects/GameObjects/3D/Model.h"

#include "Objects/SystemObjects/Camera3D.h"
#include "Objects/SystemObjects/DirectionalLight.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Objects/GameObjects/3D/Triangle3D.h"

#include "Objects/SystemObjects/Camera2D.h"
#include "Objects/Components/2D/Transform2D.h"
#include "Objects/GameObjects/2D/Triangle2D.h"

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

constexpr bool kEnableInstancingTest = true;
constexpr std::uint32_t kInstancingTestCount2D = 1024;
constexpr std::uint32_t kInstancingTestCount3D = 1024;
} // namespace

#if defined(USE_IMGUI)
GameEngine::RollingAverage_::RollingAverage_(std::size_t capacity) {
    SetCapacity(capacity);
}

void GameEngine::RollingAverage_::SetCapacity(std::size_t capacity) {
    capacity = std::max<std::size_t>(1, capacity);
    if (capacity_ == capacity) return;

    capacity_ = capacity;
    samples_.clear();
    samples_.reserve(capacity_);
    writeIndex_ = 0;
    sum_ = 0.0;
}

void GameEngine::RollingAverage_::Add(double value) {
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

double GameEngine::RollingAverage_::GetAverage() const {
    if (samples_.empty()) return 0.0;
    return sum_ / static_cast<double>(samples_.size());
}

std::size_t GameEngine::RollingAverage_::GetCount() const {
    return samples_.size();
}

void GameEngine::DrawProfilingImGui_() {
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

void GameEngine::InitializeTestObjects_() {
    if (testObjectsInitialized_) {
        return;
    }

    auto mainWindow = Window::GetWindow("Main Window");
    auto overlayWindow = Window::GetWindow("Overlay Window");
    auto *targetWindow = overlayWindow ? overlayWindow : mainWindow;

    auto attachIfPossible3D = [&](Object3DBase *obj) {
        if (!obj || !targetWindow) return;
        obj->AttachToRenderer(targetWindow, "Object3D.Solid.BlendNormal");
    };
    auto attachIfPossible2D = [&](Object2DBase *obj) {
        if (!obj || !targetWindow) return;
        obj->AttachToRenderer(targetWindow, "Object2D.DoubleSidedCulling.BlendNormal");
    };

    // 3D
    testObjects3D_.clear();
    testObjects3D_.reserve(2 + (kEnableInstancingTest ? kInstancingTestCount3D : 0));

    testObjects3D_.emplace_back(std::make_unique<Camera3D>());
    if (auto *camera = static_cast<Camera3D *>(testObjects3D_.back().get())) {
        if (auto *transformComp = camera->GetComponent3D<Transform3D>()) {
            transformComp->SetTranslate(Vector3(0.0f, 0.0f, -10.0f));
        }
    }
    attachIfPossible3D(testObjects3D_.back().get());

    testObjects3D_.emplace_back(std::make_unique<DirectionalLight>());
    if (auto *light = static_cast<DirectionalLight *>(testObjects3D_.back().get())) {
        light->SetEnabled(true);
        light->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        light->SetDirection(Vector3(0.3f, -1.0f, 0.2f));
        light->SetIntensity(1.0f);
    }
    attachIfPossible3D(testObjects3D_.back().get());

    {
        const uint32_t instanceCount = kEnableInstancingTest ? kInstancingTestCount3D : 0;
        Log(std::string("[InstancingTest] Setup started. instancing=")
                + (kEnableInstancingTest ? "true" : "false")
                + " 3DCount=" + std::to_string(instanceCount),
            LogSeverity::Info);
        for (std::uint32_t i = 0; i < instanceCount; ++i) {
            auto obj = std::make_unique<Triangle3D>();
            obj->SetName(std::string("InstancingTriangle3D_") + std::to_string(i));

            if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                const float x = (static_cast<float>(i % 16) - 8.0f) * 0.4f;
                const float y = (static_cast<float>(i / 16) - 2.0f) * 0.4f;
                tr->SetTranslate(Vector3(x, y, 0.0f));
            }
            attachIfPossible3D(obj.get());
            testObjects3D_.emplace_back(std::move(obj));
        }
    }

    {
        ModelManager::ModelHandle modelHandle
            = ModelManager::GetModelHandleFromFileName("icoSphere.obj");
        if (modelHandle != ModelManager::kInvalidHandle) {
            auto obj = std::make_unique<Model>(modelHandle);
            obj->SetName("TestModel_IcoSphere");
            if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(Vector3(2.0f, 0.0f, 0.0f));
                tr->SetScale(Vector3(0.5f, 0.5f, 0.5f));
            }
            attachIfPossible3D(obj.get());
            testObjects3D_.emplace_back(std::move(obj));
        } else {
            Log("[GameEngine] Test model 'icoShpere.obj' not found.", LogSeverity::Warning);
        }
    }

    // 2D
    testObjects2D_.clear();
    testObjects2D_.reserve(1 + (kEnableInstancingTest ? kInstancingTestCount2D : 0));

    testObjects2D_.emplace_back(std::make_unique<Camera2D>());
    attachIfPossible2D(testObjects2D_.back().get());

    {
        const uint32_t instanceCount = kEnableInstancingTest ? kInstancingTestCount2D : 0;
        Log(std::string("[InstancingTest] Setup started. instancing=")
                + (kEnableInstancingTest ? "true" : "false")
                + " 2DCount=" + std::to_string(instanceCount),
            LogSeverity::Info);
        for (std::uint32_t i = 0; i < instanceCount; ++i) {
            auto obj = std::make_unique<Triangle2D>();
            obj->SetName(std::string("InstancingTriangle2D_") + std::to_string(i));
            if (auto *tr = obj->GetComponent2D<Transform2D>()) {
                const float x = 50.0f + static_cast<float>(i % 16) * 30.0f;
                const float y = 200.0f + static_cast<float>(i / 16) * 30.0f;
                tr->SetTranslate(Vector2(x, y));
                tr->SetScale(Vector2(20.0f, 20.0f));
            }
            attachIfPossible2D(obj.get());
            testObjects2D_.emplace_back(std::move(obj));
        }
    }

    testObjectsInitialized_ = true;
}

void GameEngine::UpdateTestObjects_() {
    if (!testObjectsInitialized_) {
        return;
    }

    for (auto &obj : testObjects3D_) {
        if (!obj) continue;
        if (obj->GetName() == "Camera3D") continue;
        if (obj->GetName() == "DirectionalLight") continue;

        if (auto *transformComp = obj->GetComponent3D<Transform3D>()) {
            Vector3 rotate = transformComp->GetRotate();
            rotate.y += 0.01f;
            transformComp->SetRotate(rotate);
        }
    }
}

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

    windowsAPI_ = std::make_unique<WindowsAPI>(Passkey<GameEngine>{});
    directXCommon_ = std::make_unique<DirectXCommon>(Passkey<GameEngine>{});
    graphicsEngine_ = std::make_unique<GraphicsEngine>(Passkey<GameEngine>{}, directXCommon_.get());

    textureManager_ = std::make_unique<TextureManager>(Passkey<GameEngine>{}, directXCommon_.get(), "Assets");
    samplerManager_ = std::make_unique<SamplerManager>(Passkey<GameEngine>{}, directXCommon_.get());
    modelManager_ = std::make_unique<ModelManager>(Passkey<GameEngine>{}, "Assets");
    audioManager_ = std::make_unique<AudioManager>(Passkey<GameEngine>{}, "Assets");
    Model::SetModelManager(Passkey<GameEngine>{}, modelManager_.get());

#if defined(USE_IMGUI)
    imguiManager_ = std::make_unique<ImGuiManager>(Passkey<GameEngine>{}, windowsAPI_.get(), directXCommon_.get());
#endif

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

    InitializeTestObjects_();

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

#if defined(USE_IMGUI)
    imguiManager_.reset();
#endif

    audioManager_.reset();
    modelManager_.reset();
    samplerManager_.reset();
    textureManager_.reset();

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

    InitializeTestObjects_();
    UpdateTestObjects_();

    // Main Window をsinを使って上下に移動させる
    static float t = 0.0f;
    t += GetDeltaTime();
    auto mainWindows = Window::GetWindows("Main Window");
    if (!mainWindows.empty()) {
        auto *mainWindow = mainWindows.front();
        auto monitorInfo = windowsAPI_->QueryMonitorInfo();
        const int32_t centerY = (monitorInfo->WorkArea().bottom - monitorInfo->WorkArea().top) / 2;
        const int32_t amplitude = (monitorInfo->WorkArea().bottom - monitorInfo->WorkArea().top) / 4;
        const int32_t newY = centerY + static_cast<int32_t>(amplitude * std::sin(t));
        mainWindow->SetWindowPosition(mainWindow->GetWindowPosition().x, newY);
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
        DrawProfilingImGui_();
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