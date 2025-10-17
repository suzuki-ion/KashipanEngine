#pragma once
#include <memory>
#include "Core/WindowsAPI.h"
#include "Core/DirectXCommon.h"

namespace KashipanEngine {

/// @brief ゲームエンジンクラス
class GameEngine final {
public:
    GameEngine(const std::string &title, int32_t width, int32_t height);
    ~GameEngine();

    /// @brief WindowsAPIクラスの取得
    WindowsAPI *GetWindowsAPI() const noexcept { return windowsAPI_.get(); }
    /// @brief DirectX共通クラスの取得
    DirectXCommon *GetDirectXCommon() const noexcept { return directXCommon_.get(); }
    /// @brief メインウィンドウの取得
    Window *GetMainWindow() const noexcept { return mainWindow_; }

private:
    /// @brief WindowsAPIクラス
    std::unique_ptr<WindowsAPI> windowsAPI_;
    /// @brief DirectX共通クラス
    std::unique_ptr<DirectXCommon> directXCommon_;

    /// @brief メインウィンドウ
    Window *mainWindow_;
};

} // namespace KashipanEngine
