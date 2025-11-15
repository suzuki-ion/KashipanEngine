#pragma once
#include <memory>
#include "Core/WindowsAPI.h"
#include "Core/DirectXCommon.h"
#include "Graphics/GraphicsEngine.h"

namespace KashipanEngine {

/// @brief ゲームエンジンクラス
class GameEngine final {
public:
    /// @brief コンストラクタ
    /// @param engineSettingsPath エンジン設定ファイルパス
    GameEngine(PasskeyForGameEngineMain);
    ~GameEngine();

    /// @brief ゲームエンジン実行用関数
    /// @return 実行結果コード
    int Execute();

    /// @brief ゲームループ実行関数
    void GameLoopRun();
    /// @brief ゲームループ終了関数
    void GameLoopEnd();
    /// @brief ゲームループ一時停止関数
    void GameLoopPause();
    /// @brief ゲームループ再開関数
    void GameLoopResume();

private:
    GameEngine(const GameEngine &) = delete;
    GameEngine &operator=(const GameEngine &) = delete;
    GameEngine(GameEngine &&) = delete;
    GameEngine &operator=(GameEngine &&) = delete;

    /// @brief ゲームループ更新処理
    void GameLoopUpdate();
    /// @brief ゲームループ描画処理
    void GameLoopDraw();

    /// @brief WindowsAPIクラス
    std::unique_ptr<WindowsAPI> windowsAPI_;
    /// @brief DirectX共通クラス
    std::unique_ptr<DirectXCommon> directXCommon_;
    /// @brief グラフィックスエンジンクラス
    std::unique_ptr<GraphicsEngine> graphicsEngine_;

    /// @brief メインウィンドウ
    Window *mainWindow_ = nullptr;

    /// @brief ゲームループ実行フラグ
    bool isGameLoopRunning_ = false;
    /// @brief ゲームループ一時停止フラグ
    bool isGameLoopPaused_ = false;
};

} // namespace KashipanEngine
