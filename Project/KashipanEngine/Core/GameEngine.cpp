#include "GameEngine.h"
#include "EngineSettings.h"
#include "Core/ProjectPaths.h"
#include "Core/Window.h"
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/Translation.h"
#include "Utilities/TimeUtils.h"
#include "Graphics/ScreenBuffer.h"
#include "Graphics/ShadowMapBuffer.h"
#include "Graphics/ComputeCommandProcessor.h"
#include "Objects/Components/ParticleSystemBase.h"
#include "AppInitialize.h"

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

    if (ImGui::Begin(TranslationLabel("editor.gameengine.profiling.window"))) {
        ImGui::Text(TranslationC("editor.gameengine.fps_2f"), fps_);
        ImGui::Text(TranslationC("editor.gameengine.update_3f_ms"), updateMs_);
        ImGui::Text(TranslationC("editor.gameengine.draw_3f_ms"), drawMs_);
        if (graphicsEngine_) {
            ImGui::Text(TranslationC("editor.gameengine.draw_calls_u"), graphicsEngine_->GetLastFrameDrawCallCount());
        }
        ImGui::Separator();
        ImGui::SliderInt(TranslationLabel("editor.gameengine.profiling_sample_count"), &profilingSampleCount_, 1, 600);
        ImGui::Text(TranslationC("editor.gameengine.averages_zu_samples"), avgUpdateMs_.GetCount());
        ImGui::Text(TranslationC("editor.gameengine.avg_fps_2f"), static_cast<float>(avgFps_.GetAverage()));
        ImGui::Text(TranslationC("editor.gameengine.avg_update_3f_ms"), static_cast<float>(avgUpdateMs_.GetAverage()));
        ImGui::Text(TranslationC("editor.gameengine.avg_draw_3f_ms"), static_cast<float>(avgDrawMs_.GetAverage()));
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

    windowsAPI_ = std::make_unique<WindowsAPI>(Passkey<GameEngine>{});
    directXCommon_ = std::make_unique<DirectXCommon>(Passkey<GameEngine>{});
    ScreenBuffer::SetDirectXCommon(Passkey<GameEngine>{}, directXCommon_.get());
    ShadowMapBuffer::SetDirectXCommon(Passkey<GameEngine>{}, directXCommon_.get());
    Scene::SetDirectXCommon(Passkey<GameEngine>{}, directXCommon_.get());
    ParticleSystemBase::SetDirectXCommon(Passkey<GameEngine>{}, directXCommon_.get());
    ComputeCommandProcessor::Initialize(Passkey<GameEngine>{}, directXCommon_.get());
    graphicsEngine_ = std::make_unique<GraphicsEngine>(Passkey<GameEngine>{}, directXCommon_.get());
    Scene::SetGraphicsEngine(Passkey<GameEngine>{}, graphicsEngine_.get());

    // 各Managerには物理パスのAssetsルートを渡す。Manager内部で扱うアセットパスは
    // このルートからの相対パスになるため、プロジェクトが変わっても値は変わらない
    const std::string &assetsRoot = ProjectPaths::AssetsRoot();
    textureManager_ = std::make_unique<TextureManager>(Passkey<GameEngine>{}, directXCommon_.get(), assetsRoot);
    fontManager_ = std::make_unique<FontManager>(Passkey<GameEngine>{}, directXCommon_.get(), assetsRoot);
    bitmapFontManager_ = std::make_unique<BitmapFontManager>(Passkey<GameEngine>{}, assetsRoot);
    samplerManager_ = std::make_unique<SamplerManager>(Passkey<GameEngine>{}, directXCommon_.get());
    modelManager_ = std::make_unique<ModelManager>(Passkey<GameEngine>{}, assetsRoot);
    skeletonManager_ = std::make_unique<SkeletonManager>(Passkey<GameEngine>{}, assetsRoot);
    audioManager_ = std::make_unique<AudioManager>(Passkey<GameEngine>{}, assetsRoot);
    animationManager_ = std::make_unique<AnimationManager>(Passkey<GameEngine>{}, assetsRoot);
    materialManager_ = std::make_unique<MaterialManager>(Passkey<GameEngine>{}, assetsRoot);
    videoManager_ = std::make_unique<VideoManager>(Passkey<GameEngine>{}, directXCommon_.get(), assetsRoot);
    gifManager_ = std::make_unique<GifManager>(Passkey<GameEngine>{}, directXCommon_.get(), assetsRoot);
    input_ = std::make_unique<Input>(Passkey<GameEngine>{});
    inputCommand_ = std::make_unique<InputCommand>(Passkey<GameEngine>{}, input_.get());
    inputCommand_->LoadFromJSON(InputCommand::kDefaultSaveFilePath);
    sceneManager_ = std::make_unique<SceneManager>(Passkey<GameEngine>());
    // シーンリスト定義ファイルが存在する場合は、記載されたシーンを自動登録する
    // （AppInitializeでのRegisterScene呼び出しが無くてもシーンを定義できるようにするため）
    sceneManager_->LoadSceneList();
    // グローバルシーン変数定義ファイルが存在する場合は、保存されている変数を読み込む
    sceneManager_->LoadGlobalSceneVariables();

    context_.engine = this;
    context_.sceneManager = sceneManager_.get();
    context_.inputCommand = inputCommand_.get();

    Scene::SetEnginePointers(
        Passkey<GameEngine>{},
        audioManager_.get(),
        modelManager_.get(),
        skeletonManager_.get(),
        samplerManager_.get(),
        textureManager_.get(),
        animationManager_.get(),
        materialManager_.get(),
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

#if defined(USE_IMGUI)
    imguiManager_ = std::make_unique<ImGuiManager>(Passkey<GameEngine>{}, windowsAPI_.get(), directXCommon_.get());
#endif

    //--------- ゲームループ終了条件 ---------//

    gameLoopEndConditionFunction_ = [this]() {
#ifdef USE_IMGUI
        return Window::GetWindow("ImGui Window") == nullptr;
#else
        return isGameLoopRunning_ == false;
#endif
    };

    AppInitialize(context_);

    // AppInitializeでシーン切り替えが指定されなかった場合は、
    // シーンリスト定義ファイルのスタートアップシーンへ自動的に切り替える
    if (sceneManager_ && !sceneManager_->HasPendingSceneChange()) {
        if (!sceneManager_->ChangeToStartupScene()) {
            // シーンが1つも登録されていない（Emptyテンプレート等でSceneList.jsonが空の）場合、
            // ここでシーン切り替えを予約しないと currentScene_ が永久にnullのままになる。
            // SceneEditor等のImGuiウィンドウはほぼ全てScene配下にぶら下がっているため、
            // これを放置するとエンジンプロファイリング以外の編集用ウィンドウが一切出せなくなり、
            // シーンを新規作成するためのメニューにすら到達できなくなってしまう。
            // 最低限編集を始められるよう、空のシーンを登録して切り替えておく
            constexpr const char *kFallbackSceneName = "New Scene";
            if (sceneManager_->RegisterScene(kFallbackSceneName)) {
                sceneManager_->ChangeScene(kFallbackSceneName);
            }
        }
    }

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
    ComputeCommandProcessor::Finalize(Passkey<GameEngine>{});

    sceneManager_.reset();
    if (inputCommand_) {
        inputCommand_->SaveToJSON(InputCommand::kDefaultSaveFilePath);
    }
    inputCommand_.reset();
    input_.reset();

#if defined(USE_IMGUI)
    imguiManager_.reset();
#endif

    // VideoManagerはAudioManager/TextureManager双方へ登録したハンドルを破棄側で解放するため、
    // その2つより先に破棄する
    videoManager_.reset();
    // GifManager自体はTextureManagerへ何も登録しない（登録・解除はGifSourceが自分の
    // Finalize()で行う。既にsceneManager_.reset()済みのためこの時点では残っていない）が、
    // 動画・音声・マテリアル系の各Managerと並びを揃えるためここで破棄する
    gifManager_.reset();
    materialManager_.reset();
    audioManager_.reset();
    animationManager_.reset();
    skeletonManager_.reset();
    modelManager_.reset();
    samplerManager_.reset();
    fontManager_.reset();
    bitmapFontManager_.reset();
    textureManager_.reset();

    Scene::SetGraphicsEngine(Passkey<GameEngine>{}, nullptr);
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
    if (videoManager_) {
        videoManager_->Update();
    }
#if defined(USE_IMGUI)
    imguiManager_->BeginFrame({});
#endif

    if (sceneManager_) {
        sceneManager_->Update(Passkey<GameEngine>{});
#if defined(USE_IMGUI)
        sceneManager_->ShowImGui(Passkey<GameEngine>{});
#endif
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

    {
        SceneContext *sceneContext = nullptr;
        if (sceneManager_) {
            if (const auto *currentScene = sceneManager_->GetCurrentScene()) {
                sceneContext = currentScene->GetSceneContext();
            }
        }
        graphicsEngine_->RenderFrame({}, sceneContext);
    }

#if defined(USE_IMGUI)
    if (imguiManager_) {
        DrawProfilingImGui();
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
    static size_t windowCount = 0;

    while (!gameLoopEndConditionFunction_()) {
        // プロジェクトの切り替えなどによるアプリケーション自体の終了要求
        // （エディタービルドでも消費されずにここまで届く）
        if (sIsQuitRequested) break;
#if !defined(USE_IMGUI)
        // シーンやスクリプトからの終了要求でゲームループを抜ける
        // （エディタービルドでは再生停止要求としてScene側で消費されるため、ここでは見ない）
        if (sIsExitGameLoopRequested) break;
#endif
        GameLoopUpdate();
        GameLoopDraw();

        if (sceneManager_ && sceneManager_->CommitPendingSceneChange({})) {
            graphicsEngine_->ReleaseRendererResources(Passkey<GameEngine>{});
            // シーン構築中のアセット読み込みで実時間が飛んでいるため、
            // 次フレームのデルタタイムへその時間が混入しないようにする
            ResetDeltaTime({});
        }
        // スワップチェーンの破棄は、それが紐付くウィンドウ(HWND)を破棄するより必ず先に行うこと。
        // Window::CommitDestroy()はDestroyWindow()を同期的に呼ぶため、先にHWNDを破棄してしまうと
        // まだ生きているIDXGISwapChainが破棄済みウィンドウを参照した状態になる。この状態でPresent/
        // 破棄が走ると、DWM側の合成状態が壊れてGPUハング（TDR）→次フレームのPresent失敗を
        // 引き起こしうる（Play/StopでNormalWindowObject等がウィンドウごと即座に作り直される際に
        // 顕在化しやすい）
        directXCommon_->AllDestroyPendingSwapChains({});
        Window::CommitDestroy({});
        ScreenBuffer::CommitDestroy({});
        ShadowMapBuffer::CommitDestroy({});
        VideoManager::CommitPendingDestroy({});
        GifManager::CommitPendingDestroy({});

        if (windowCount > Window::GetWindowCount()) {
            isGameLoopRunning_ = Window::GetWindowCount() != 0;
        }
        windowCount = Window::GetWindowCount();
    }
    return 0;
}

} // namespace KashipanEngine
