#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>
#include <optional>

#include "Core/WindowsAPI/WindowDescriptor.h"
#include "Core/WindowsAPI/WindowMessage.h"
#include "Core/WindowsAPI/WindowSize.h"

#include "Core/WindowsAPI/WindowEvents/IWindowEvent.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class GameEngine;
class WindowsAPI;
class DirectXCommon;

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
    static inline WindowsAPI *sWindowsAPI = nullptr;
    static inline DirectXCommon *sDirectXCommon = nullptr;

public:
    static void SetWindowsAPI(Passkey<GameEngine>, WindowsAPI *windowsAPI) { sWindowsAPI = windowsAPI; }
    static void SetDirectXCommon(Passkey<GameEngine>, DirectXCommon *directXCommon) { sDirectXCommon = directXCommon; }
    static void SetDefaultParams(Passkey<GameEngine>, const std::string &title, int32_t width, int32_t height, DWORD style, const std::string &iconPath);
    
    /// @brief 全ウィンドウ破棄
    static void AllDestroy(Passkey<GameEngine>);
    /// @brief HWNDからウィンドウインスタンスを取得
    /// @param hwnd ウィンドウハンドル
    /// @return ウィンドウインスタンスへのポインタ。存在しない場合はnullptr
    static Window *GetWindow(HWND hwnd);
    /// @brief ウィンドウタイトルからウィンドウインスタンスを取得
    /// @param title ウィンドウタイトル
    /// @return 一致するウィンドウインスタンスへのポインタのリスト。存在しない場合は空のリスト
    static std::vector<Window *> GetWindows(const std::string &title);

    /// @brief 指定のHWNDのウィンドウが存在するか
    /// @param hwnd ウィンドウハンドル
    static bool IsExist(HWND hwnd);
    /// @brief 指定のウィンドウインスタンスが存在するか
    /// @param window ウィンドウインスタンスへのポインタ
    static bool IsExist(Window *window);
    /// @brief 指定のウィンドウタイトルのウィンドウが存在するか
    /// @param title ウィンドウタイトル
    static bool IsExist(const std::string &title);

    /// @brief ウィンドウ更新処理
    static void Update(Passkey<GameEngine>);

    /// @brief コンストラクタ（Window限定）
    /// @param title ウィンドウタイトル
    /// @param width ウィンドウ幅
    /// @param height ウィンドウ高さ
    /// @param windowStyle ウィンドウスタイル
    /// @param iconPath アイコンパス
    Window(Passkey<Window>,
        const std::wstring &title = L"GameWindow",
        int32_t width = 1280,
        int32_t height = 720,
        DWORD windowStyle = WS_OVERLAPPEDWINDOW,
        const std::wstring &iconPath = L"");
    ~Window();
    
    /// @brief ウィンドウ作成
    /// @param title ウィンドウタイトル
    /// @param width ウィンドウ幅
    /// @param height ウィンドウ高さ
    /// @param style ウィンドウスタイル
    /// @param iconPath アイコンパス
    /// @return ウィンドウインスタンスへのポインタ
    static Window *Create(const std::string &title = "",
        int32_t width = 0,
        int32_t height = 0,
        DWORD style = 0,
        const std::string &iconPath = "");
    void Destroy();

    /// @brief ウィンドウプロシージャから呼び出されるイベント処理
    std::optional<LRESULT> HandleEvent(Passkey<WindowsAPI>, UINT msg, WPARAM wparam, LPARAM lparam);
    
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

    /// @brief 指定のウィンドウスタイルを持っているかどうかをチェック
    bool HasWindowStyle(DWORD style) const noexcept { return (descriptor_.windowStyle & style) != 0; }
    /// @brief 指定のメッセージが来ているかどうかをチェック
    bool HasMessage(UINT msg) const { return messages_.find(msg) != messages_.end(); }
    /// @brief 指定のメッセージの情報を取得
    const WindowMessage &GetWindowMessage(UINT msg) const;

    /// @brief ウィンドウがアクティブかどうかを取得する
    bool IsActive() const noexcept { return HasMessage(WM_ACTIVATE); }
    /// @brief ウィンドウがサイズ変更中かどうかを取得する
    bool IsResizing() const noexcept { return HasMessage(WM_ENTERSIZEMOVE) && !HasMessage(WM_EXITSIZEMOVE); }
    /// @brief ウィンドウが閉じられたかどうかを取得する
    bool IsClosed() const noexcept { return HasMessage(WM_CLOSE); }
    /// @brief ウィンドウが破棄されたかどうかを取得する
    bool IsDestroyed() const noexcept { return HasMessage(WM_DESTROY); }
    /// @brief ウィンドウがサイズ変更されたかどうかを取得する
    bool IsResized() const noexcept { return HasMessage(WM_SIZE); }
    /// @brief ウィンドウが最小化されたかどうかを取得する
    bool IsMinimized() const noexcept { return HasMessage(WM_SIZE) && (messages_.at(WM_SIZE).wparam == SIZE_MINIMIZED); }
    /// @brief ウィンドウが最大化されたかどうかを取得する
    bool IsMaximized() const noexcept { return HasMessage(WM_SIZE) && (messages_.at(WM_SIZE).wparam == SIZE_MAXIMIZED); }

private:
    static constexpr size_t kMaxMessages = 512;
    static inline std::string windowDefaultTitle = "KashipanEngine";
    static inline int32_t windowDefaultWidth = 1280;
    static inline int32_t windowDefaultHeight = 720;
    static inline DWORD windowDefaultStyle = WS_OVERLAPPEDWINDOW;
    static inline std::string windowDefaultIconPath = "";

    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;
    Window(Window &&) = delete;
    Window &operator=(Window &&) = delete;

    /// @brief ウィンドウの初期化
    /// @param windowProc ウィンドウプロシージャ
    /// @param title ウィンドウタイトル
    /// @param width ウィンドウ幅
    /// @param height ウィンドウ高さ
    /// @param windowStyle ウィンドウスタイル
    /// @param iconPath アイコンのパス
    /// @return 初期化成功かどうか
    bool InitializeWindow(
        WNDPROC windowProc,
        const std::wstring &title,
        int32_t width,
        int32_t height,
        DWORD windowStyle,
        const std::wstring &iconPath);

    /// @brief ウィンドウのメッセージクリア
    void ClearMessages() { messages_.clear(); }
    /// @brief メッセージ処理
    void ProcessMessage();

    /// @brief ウィンドウのクリーンアップ
    void Cleanup();
    /// @brief アスペクト比の計算
    void CalculateAspectRatio();
    /// @brief ウィンドウサイズの調整
    void AdjustWindowSize();

    // ウィンドウ関連
    WindowDescriptor descriptor_{};
    // サイズ関連
    WindowSize size_{};
    // メッセージ関連
    std::unordered_map<UINT, WindowMessage> messages_;
    
    // 状態管理
    SizeChangeMode sizeChangeMode_ = SizeChangeMode::Normal;
    WindowMode windowMode_ = WindowMode::Window;

    // 内部で保持するワイド文字列（WinAPIのクラス名/タイトル用）
    std::wstring titleW_ = L"";

    // イベントハンドラマップ
    std::unordered_map<UINT, std::unique_ptr<IWindowEvent>> eventHandlers_;
};

} // namespace KashipanEngine} // namespace KashipanEngine