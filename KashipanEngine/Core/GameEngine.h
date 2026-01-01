#pragma once
#include <memory>
#include <functional>
#include <vector>
#include <utility>

#include "Core/WindowsAPI.h"
#include "Core/DirectXCommon.h"
#include "Graphics/GraphicsEngine.h"
#include "Assets/TextureManager.h"
#include "Assets/SamplerManager.h"
#include "Assets/ModelManager.h"
#include "Assets/AudioManager.h"
#include "Objects/Object2DBase.h"
#include "Objects/Object3DBase.h"

#if defined(USE_IMGUI)
#include "Debug/ImGuiManager.h"
#endif

namespace KashipanEngine {

/// @brief ゲームエンジンクラス
class GameEngine final {
public:
    /// @brief コンストラクタ
    /// @param engineSettingsPath エンジン設定ファイルパス
    GameEngine(PasskeyForGameEngineMain);
    ~GameEngine();

    GameEngine(const GameEngine &) = delete;
    GameEngine &operator=(const GameEngine &) = delete;
    GameEngine(GameEngine &&) = delete;
    GameEngine &operator=(GameEngine &&) = delete;

    /// @brief ゲームエンジン実行用関数
    /// @return 実行結果コード
    int Execute(PasskeyForGameEngineMain);

    /// @brief ゲームループ実行関数
    void GameLoopRun();
    /// @brief ゲームループ終了関数
    void GameLoopEnd();
    /// @brief ゲームループ一時停止関数
    void GameLoopPause();
    /// @brief ゲームループ再開関数
    void GameLoopResume();

    /// @brief ゲームループ終了条件設定
    void SetGameLoopEndCondition(const std::function<bool()> &func) {
        gameLoopEndConditionFunction_ = func;
    }

private:
    /// @brief ゲームループ更新処理
    void GameLoopUpdate();
    /// @brief ゲームループ描画処理
    void GameLoopDraw();

    void InitializeTestObjects_();
    void UpdateTestObjects_();

#if defined(USE_IMGUI)
    class RollingAverage_ {
    public:
        explicit RollingAverage_(std::size_t capacity = 60);
        void SetCapacity(std::size_t capacity);
        void Add(double value);
        double GetAverage() const;
        std::size_t GetCount() const;

    private:
        std::vector<double> samples_{};
        std::size_t capacity_ = 1;
        std::size_t writeIndex_ = 0;
        double sum_ = 0.0;
    };

    void DrawProfilingImGui_();

    float updateMs_ = 0.0f;
    float drawMs_ = 0.0f;
    float fps_ = 0.0f;
    int profilingSampleCount_ = 60;
    RollingAverage_ avgUpdateMs_{60};
    RollingAverage_ avgDrawMs_{60};
    RollingAverage_ avgFps_{60};
#endif

    /// @brief WindowsAPIクラス
    std::unique_ptr<WindowsAPI> windowsAPI_;
    /// @brief DirectX共通クラス
    std::unique_ptr<DirectXCommon> directXCommon_;
    /// @brief グラフィックスエンジンクラス
    std::unique_ptr<GraphicsEngine> graphicsEngine_;

    bool testObjectsInitialized_ = false;
    std::vector<std::unique_ptr<Object3DBase>> testObjects3D_;
    std::vector<std::unique_ptr<Object2DBase>> testObjects2D_;

    /// @brief テクスチャ管理クラス
    std::unique_ptr<TextureManager> textureManager_;
    /// @brief サンプラ管理クラス
    std::unique_ptr<SamplerManager> samplerManager_;
    /// @brief モデル管理クラス
    std::unique_ptr<ModelManager> modelManager_;
    /// @brief 音声管理クラス
    std::unique_ptr<AudioManager> audioManager_;

#if defined(USE_IMGUI)
    /// @brief ImGui 管理クラス
    std::unique_ptr<ImGuiManager> imguiManager_;
#endif

    /// @brief ゲームループ実行フラグ
    bool isGameLoopRunning_ = false;
    /// @brief ゲームループ一時停止フラグ
    bool isGameLoopPaused_ = false;

    /// @brief ゲームループ終了条件関数
    std::function<bool()> gameLoopEndConditionFunction_;

    /// @brief ウィンドウ配列
    std::vector<Window *> windows_;
};

} // namespace KashipanEngine
