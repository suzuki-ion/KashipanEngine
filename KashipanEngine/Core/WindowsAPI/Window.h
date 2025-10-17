#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>
#include <optional>
#include "Core/WindowsAPI/WindowDescriptor.h"
#include "Core/WindowsAPI/WindowSize.h"
#include "Core/WindowsAPI/WindowEvents/IWindowEvent.h"

namespace KashipanEngine {

class WindowsAPI;
class DirectX;

/// @brief サイズ変更モード
enum class SizeChangeMode {
    None,           // サイズ変更不可
    Normal,         // 自由変更
    FixedAspect,    // アスペクト比固定
};

/// @brief ウィンドウモード
enum class WindowMode {
    Window,         // ウィンドウ
    FullScreen,     // フルスクリーン
};

/// @brief ウィンドウ用クラス
class Window final {
    friend class IWindowEvent;
public:
    class Factory {
        friend class WindowsAPI;
        static std::unique_ptr<Window> Create(
            const std::wstring &title = L"GameWindow",
            int32_t width = 1280,
            int32_t height = 720,
            DWORD windowStyle = WS_OVERLAPPEDWINDOW,
            const std::wstring &iconPath = L"");
    };
    class ProcedureHandler {
        friend class WindowsAPI;
        static std::optional<LRESULT> HandleWindowEvent(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    };
    ~Window();

    /// @brief メッセージ処理
    /// @return メッセージ処理結果。falseの場合は終了
    bool ProcessMessage();
    
    /// @brief サイズ変更モードを設定する
    /// @param sizeChangeMode サイズ変更モード
    void SetSizeChangeMode(SizeChangeMode sizeChangeMode);
    /// @brief ウィンドウモードを設定する
    /// @param windowMode ウィンドウモード
    void SetWindowMode(WindowMode windowMode);
    /// @brief ウィンドウタイトルを設定する
    /// @param title ウィンドウタイトル
    void SetWindowTitle(const std::wstring &title);
    /// @brief ウィンドウサイズを設定する
    /// @param width クライアント幅
    /// @param height クライアント高さ
    void SetWindowSize(int32_t width, int32_t height);
    /// @brief ウィンドウ位置を設定する
    /// @param x ウィンドウ左上のX座標
    /// @param y ウィンドウ左上のY座標
    void SetWindowPosition(int32_t x, int32_t y);
    /// @brief ウィンドウイベントを登録する
    /// @param eventHandler イベントハンドラ
    void RegisterWindowEvent(const std::unique_ptr<IWindowEvent> &eventHandler);
    /// @brief ウィンドウイベントの登録解除
    /// @param msg メッセージ
    void UnregisterWindowEvent(UINT msg);

    /// @brief サイズ変更モードを取得する
    SizeChangeMode GetSizeChangeMode() const noexcept { return sizeChangeMode_; }
    /// @brief ウィンドウモードを取得する
    WindowMode GetWindowMode() const noexcept { return windowMode_; }
    /// @brief ウィンドウハンドルを取得する
    HWND GetWindowHandle() const noexcept { return descriptor_.hwnd; }
    /// @brief ウィンドウクラスを取得する
    const WNDCLASS& GetWindowClass() const noexcept { return descriptor_.wc; }
    /// @brief クライアント幅を取得する
    int32_t GetClientWidth() const noexcept { return size_.clientWidth; }
    /// @brief クライアント高さを取得する
    int32_t GetClientHeight() const noexcept { return size_.clientHeight; }
    /// @brief アスペクト比を取得する
    float GetAspectRatio() const noexcept { return size_.aspectRatio; }

private:
    friend class Factory;
    friend class ProcedureHandler;

    /// @brief コンストラクタ
    /// @param title ウィンドウタイトル
    /// @param width ウィンドウ幅
    /// @param height ウィンドウ高さ
    /// @param windowStyle ウィンドウスタイル
    /// @param windowsAPI WindowsAPIクラスへのポインタ
    Window(const std::wstring &title = L"GameWindow",
        int32_t width = 1280,
        int32_t height = 720,
        DWORD windowStyle = WS_OVERLAPPEDWINDOW,
        const std::wstring &iconPath = L"");

    /// @brief ウィンドウの初期化
    /// @param title ウィンドウタイトル
    /// @param width ウィンドウ幅
    /// @param height ウィンドウ高さ
    /// @param windowStyle ウィンドウスタイル
    /// @param iconPath アイコンのパス
    /// @return 初期化成功かどうか
    bool InitializeWindow(const std::wstring &title,
        int32_t width,
        int32_t height,
        DWORD windowStyle,
        const std::wstring &iconPath);

    /// @brief ウィンドウのクリーンアップ
    void Cleanup();
    /// @brief アスペクト比の計算
    void CalculateAspectRatio();
    /// @brief ウィンドウサイズの調整
    void AdjustWindowSize();

    /// @brief ウィンドウプロシージャから呼び出されるイベント処理
    std::optional<LRESULT> HandleEvent(UINT msg, WPARAM wparam, LPARAM lparam);

    // ウィンドウ関連
    WindowDescriptor descriptor_{};
    // サイズ関連
    WindowSize size_{};
    
    // 状態管理
    SizeChangeMode sizeChangeMode_ = SizeChangeMode::Normal;
    WindowMode windowMode_ = WindowMode::Window;

    // 内部で保持するワイド文字列（WinAPIのクラス名/タイトル用）
    std::wstring titleW_ = L"";

    // イベントハンドラマップ
    std::unordered_map<UINT, std::unique_ptr<IWindowEvent>> eventHandlers_;
};

} // namespace KashipanEngine