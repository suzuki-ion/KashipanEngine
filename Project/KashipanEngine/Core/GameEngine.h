#pragma once
#include <memory>
#include <functional>
#include <vector>
#include <utility>

#include "Core/WindowsAPI.h"
#include "Core/DirectXCommon.h"
#include "Debug/Logger.h"
#include "Graphics/GraphicsEngine.h"
#include "Assets/TextureManager.h"
#include "Assets/FontManager.h"
#include "Assets/BitmapFontManager.h"
#include "Assets/SamplerManager.h"
#include "Assets/ModelManager.h"
#include "Assets/SkeletonManager.h"
#include "Assets/AudioManager.h"
#include "Assets/AnimationManager.h"
#include "Assets/MaterialManager.h"
#include "Assets/VideoManager.h"
#include "Assets/GifManager.h"
#include "Objects/EmptyObject.h"
#include "Input/Input.h"
#include "Input/InputCommand.h"
#include "Graphics/ScreenBuffer.h"
#include "Scene/SceneManager.h"

#if defined(USE_IMGUI)
#include "Debug/ImGuiManager.h"
#endif

namespace KashipanEngine {

/// @brief ゲームエンジンクラス
class GameEngine final {
public:
    struct Context {
        SceneManager *sceneManager = nullptr;
        InputCommand *inputCommand = nullptr;

        void SetGameLoopEndCondition(const std::function<bool()> &func) const {
            LogScope scope;
            if (!engine) return;
            engine->SetGameLoopEndCondition(func);
        }

    private:
        friend class GameEngine;
        GameEngine *engine = nullptr;
    };

    /// @brief コンストラクタ
    /// @param engineSettingsPath エンジン設定ファイルパス
    GameEngine(PasskeyForGameEngineMain);
    ~GameEngine();

    GameEngine(const GameEngine &) = delete;
    GameEngine &operator=(const GameEngine &) = delete;
    GameEngine(GameEngine &&) = delete;
    GameEngine &operator=(GameEngine &&) = delete;

    /// @brief ゲームエンジンクラスからコンテキストを取得
    const Context &GetContext() const { return context_; }

    /// @brief ゲームエンジン実行用関数
    /// @return 実行結果コード
    int Execute(PasskeyForGameEngineMain);

    /// @brief ゲームループ終了条件設定
    void SetGameLoopEndCondition(const std::function<bool()> &func) {
        gameLoopEndConditionFunction_ = func;
    }

    //==================================================
    // ゲームループの終了要求
    //==================================================
    // シーン・シーンコンテキスト・スクリプトなど、GameEngineへの参照を持たない場所から
    // ゲームループの終了を指示するための静的なフラグ。
    // 非エディタービルドではゲームループ（アプリケーション）が終了する。
    // エディタービルド（USE_IMGUI）ではエディター自体は閉じず、再生停止（PlayStop）の要求として扱われる。

    /// @brief ゲームループの終了を要求する（エディター実行時は再生停止の要求になる）
    static void RequestExitGameLoop() noexcept { sIsExitGameLoopRequested = true; }
    /// @brief ゲームループの終了要求が出ているかを取得する
    static bool IsExitGameLoopRequested() noexcept { return sIsExitGameLoopRequested; }
    /// @brief ゲームループの終了要求を取り下げる（エディターが要求を消費する際にも使用する）
    static void ClearExitGameLoopRequest() noexcept { sIsExitGameLoopRequested = false; }

    //==================================================
    // アプリケーション自体の終了要求
    //==================================================
    // RequestExitGameLoop はエディタービルドでは再生停止として消費されるため、
    // 「エディターごと終了させたい」場合はこちらを使う。
    // プロジェクトの切り替えのように、後始末を済ませてから終了したい処理から呼ぶ。

    /// @brief アプリケーション自体の終了を要求する（エディタービルドでもエディターが閉じる）
    static void RequestQuit() noexcept { sIsQuitRequested = true; }
    /// @brief アプリケーションの終了要求が出ているかを取得する
    static bool IsQuitRequested() noexcept { return sIsQuitRequested; }

private:
    /// @brief ゲームループの終了要求フラグ
    static inline bool sIsExitGameLoopRequested = false;
    /// @brief アプリケーション自体の終了要求フラグ
    static inline bool sIsQuitRequested = false;

    /// @brief ゲームループ更新処理
    void GameLoopUpdate();
    /// @brief ゲームループ描画処理
    void GameLoopDraw();

#if defined(USE_IMGUI)
    class RollingAverage {
    public:
        explicit RollingAverage(std::size_t capacity = 60);
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

    void DrawProfilingImGui();

    float updateMs_ = 0.0f;
    float drawMs_ = 0.0f;
    float fps_ = 0.0f;
    int profilingSampleCount_ = 60;
    RollingAverage avgUpdateMs_{60};
    RollingAverage avgDrawMs_{60};
    RollingAverage avgFps_{60};
#endif

    Context context_;
    std::unique_ptr<SceneManager> sceneManager_;

    /// @brief WindowsAPIクラス
    std::unique_ptr<WindowsAPI> windowsAPI_;
    /// @brief DirectX共通クラス
    std::unique_ptr<DirectXCommon> directXCommon_;
    /// @brief グラフィックスエンジンクラス
    std::unique_ptr<GraphicsEngine> graphicsEngine_;

    /// @brief テクスチャ管理クラス
    std::unique_ptr<TextureManager> textureManager_;
    /// @brief フォント管理クラス
    std::unique_ptr<FontManager> fontManager_;
    /// @brief ビットマップフォント(BMFont/.fnt)管理クラス
    std::unique_ptr<BitmapFontManager> bitmapFontManager_;
    /// @brief サンプラ管理クラス
    std::unique_ptr<SamplerManager> samplerManager_;
    /// @brief モデル管理クラス
    std::unique_ptr<ModelManager> modelManager_;
    /// @brief スケルトン管理クラス
    std::unique_ptr<SkeletonManager> skeletonManager_;
    /// @brief 音声管理クラス
    std::unique_ptr<AudioManager> audioManager_;
    /// @brief アニメーション管理クラス
    std::unique_ptr<AnimationManager> animationManager_;
    /// @brief マテリアル管理クラス
    std::unique_ptr<MaterialManager> materialManager_;
    /// @brief 動画管理クラス
    std::unique_ptr<VideoManager> videoManager_;
    /// @brief GIFアニメーション管理クラス
    std::unique_ptr<GifManager> gifManager_;

#if defined(USE_IMGUI)
    /// @brief ImGui 管理クラス
    std::unique_ptr<ImGuiManager> imguiManager_;
#endif

    /// @brief ゲームループ終了条件関数
    std::function<bool()> gameLoopEndConditionFunction_;
    bool isGameLoopRunning_ = true;

    /// @brief ウィンドウ配列
    std::vector<Window *> windows_;

    /// @brief 入力クラス
    std::unique_ptr<Input> input_;
    /// @brief 入力コマンドクラス
    std::unique_ptr<InputCommand> inputCommand_;
};

} // namespace KashipanEngine
