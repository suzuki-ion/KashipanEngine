#pragma once
#include <Windows.h>
#include <string>
#include <memory>
#include <vector>
#include "Utilities/Passkeys.h"
#include "Core/WindowsAPI/Window.h"

namespace KashipanEngine {

class GameEngine;

/// @brief WindowsAPI用クラス
class WindowsAPI final {
    /// @brief ウィンドウプロシージャ
    /// @param hwnd イベントの発生したウィンドウのハンドル
    /// @param msg メッセージ
    /// @param wparam wパラメータ
    /// @param lparam lパラメータ
    /// @return メッセージ処理結果
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

public:
    /// @brief コンストラクタ（GameEngine専用）
    /// @param defaultTitle デフォルトのウィンドウタイトル
    /// @param defaultWidth デフォルトのウィンドウ幅
    /// @param defaultHeight デフォルトのウィンドウ高さ
    /// @param defaultWindowStyle デフォルトのウィンドウスタイル
    /// @param defaultIconPath デフォルトのアイコンパス
    WindowsAPI(Passkey<GameEngine>,
        const std::string &defaultTitle = "GameWindow",
        int32_t defaultWidth = 1280,
        int32_t defaultHeight = 720,
        DWORD defaultWindowStyle = WS_OVERLAPPEDWINDOW,
        const std::string &defaultIconPath = "");
    ~WindowsAPI();
    WindowsAPI(const WindowsAPI &) = delete;
    WindowsAPI &operator=(const WindowsAPI &) = delete;
    WindowsAPI(WindowsAPI &&) = delete;
    WindowsAPI &operator=(WindowsAPI &&) = delete;

    /// @brief WindowsAPI更新処理
    void Update(Passkey<GameEngine>);

    /// @brief ウィンドウの作成
    /// @param title ウィンドウタイトル
    /// @param width ウィンドウ幅
    /// @param height ウィンドウ高さ
    /// @param style ウィンドウスタイル
    /// @param iconPath アイコンのパス
    /// @return 作成したウィンドウへのポインタ。失敗した場合はnullptr
    Window *CreateWindowInstance(
        const std::string &title = "",
        int32_t width = 0,
        int32_t height = 0,
        DWORD style = 0,
        const std::string &iconPath = "");

    /// @brief ウィンドウの破棄
    /// @param window 破棄するウィンドウへのポインタ
    /// @return 破棄成功かどうか
    bool DestroyWindowInstance(Window *window);

private:
    // デフォルトウィンドウパラメータ
    std::string defaultTitle_;
    int32_t defaultWidth_;
    int32_t defaultHeight_;
    DWORD defaultWindowStyle_;
    std::string defaultIconPath_;

    /// @brief ウィンドウ管理用のコンテナ
    std::vector<std::unique_ptr<Window>> windows_;
};

} // namespace KashipanEngine