#pragma once
#include <Windows.h>
#include <string>
#include <memory>
#include <vector>

namespace KashipanEngine {

class GameEngine;
class Window;

/// @brief WindowsAPI用クラス
class WindowsAPI final {
public:
    /// @brief ウィンドウプロシージャ
    /// @param hwnd イベントの発生したウィンドウのハンドル
    /// @param msg メッセージ
    /// @param wparam wパラメータ
    /// @param lparam lパラメータ
    /// @return メッセージ処理結果
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    /// @brief コンストラクタ（GameEngine専用）
    WindowsAPI(Passkey<GameEngine>);
    ~WindowsAPI();
    WindowsAPI(const WindowsAPI &) = delete;
    WindowsAPI &operator=(const WindowsAPI &) = delete;
    WindowsAPI(WindowsAPI &&) = delete;
    WindowsAPI &operator=(WindowsAPI &&) = delete;

    /// @brief ウィンドウの登録（Window限定）
    /// @param window ウィンドウへのポインタ
    /// @return 成功したらtrue、失敗したらfalse
    bool RegisterWindow(Passkey<Window>, Window *window);

    /// @brief ウィンドウの登録解除（Window限定）
    /// @param hwnd ウィンドウハンドル
    /// @return 成功したらtrue、失敗したらfalse
    bool UnregisterWindow(Passkey<Window>, HWND hwnd);
};

} // namespace KashipanEngine