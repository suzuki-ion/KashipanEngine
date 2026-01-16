#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>
#include <optional>
#include <variant>
#include <type_traits>
#include <vector>
#include <d3d12.h>

#include "Core/WindowsAPI/WindowDescriptor.h"
#include "Core/WindowsAPI/WindowMessage.h"
#include "Core/WindowsAPI/WindowSize.h"
#include "Core/WindowsAPI/WindowEvents/DefaultEvents.h"

namespace KashipanEngine {

class GameEngine;
class WindowsAPI;
class DirectXCommon;
class DX12SwapChain;
class GraphicsEngine;
class PipelineManager;
class Renderer;
class ImGuiManager;
class Object2DBase;
class Object3DBase;
class ScreenBuffer;

/// @brief ウィンドウの種類
enum class WindowType {
    Normal,         // 通常ウィンドウ
    Overlay,        // オーバーレイウィンドウ
};

/// @brief ウィンドウモード
enum class WindowMode {
    Window,         // ウィンドウ
    FullScreen,     // フルスクリーン
};

/// @brief サイズ変更モード
enum class SizeChangeMode {
    None,           // サイズ変更不可
    Normal,         // 自由変更
    FixedAspect,    // アスペクト比固定
};

/// @brief ウィンドウ用クラス
class Window final {
    friend class IWindowEvent;
    static inline WindowsAPI *sWindowsAPI = nullptr;
    static inline DirectXCommon *sDirectXCommon = nullptr;
    static inline PipelineManager *sPipelineManager = nullptr;
    static inline Renderer *sRenderer = nullptr;

    // 値保持する既定イベント + 拡張イベント(unique_ptr) をまとめた variant
    using Events = std::variant<
        WindowDefaultEvent::ActivateEvent,
        WindowDefaultEvent::ClickThroughEvent,
        WindowDefaultEvent::CloseEvent,
        WindowDefaultEvent::DestroyEvent,
        WindowDefaultEvent::EnterSizeMoveEvent,
        WindowDefaultEvent::ExitSizeMoveEvent,
        WindowDefaultEvent::GetMinMaxInfoEvent,
        WindowDefaultEvent::SizeEvent,
        WindowDefaultEvent::SizingEvent,
        WindowDefaultEvent::SysCommandCloseEvent,
        WindowDefaultEvent::SysCommandCloseEventSimple,
        std::unique_ptr<IWindowEvent>
    >;

    // Variant に型が含まれるかの簡易メタ関数
    template<class Q, class V>
    struct VariantContains : std::false_type {};
    template<class Q, class... Ts>
    struct VariantContains<Q, std::variant<Ts...>> : std::bool_constant<(std::is_same_v<std::remove_cv_t<std::remove_reference_t<Q>>, Ts> || ...)> {};

    template<class T>
    static constexpr bool IsDefaultEventV =
        VariantContains<std::remove_cv_t<std::remove_reference_t<T>>, Events>::value &&
        std::is_base_of_v<IWindowEvent, std::remove_cv_t<std::remove_reference_t<T>>> &&
        !std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, std::unique_ptr<IWindowEvent>>;

    template<class T>
    static constexpr bool IsUserEventV =
        std::is_base_of_v<IWindowEvent, std::remove_cv_t<std::remove_reference_t<T>>> && !IsDefaultEventV<T>;

public:
    static void SetWindowsAPI(Passkey<GameEngine>, WindowsAPI *windowsAPI) { sWindowsAPI = windowsAPI; }
    static void SetDirectXCommon(Passkey<GameEngine>, DirectXCommon *directXCommon) { sDirectXCommon = directXCommon; }
    static void SetDefaultParams(Passkey<GameEngine>, const std::string &title, int32_t width, int32_t height, DWORD style, const std::string &iconPath);
    static void SetPipelineManager(Passkey<GraphicsEngine>, PipelineManager *pm) { sPipelineManager = pm; }
    static void SetRenderer(Passkey<GraphicsEngine>, Renderer *renderer) { sRenderer = renderer; }

    /// @brief 全ウィンドウ破棄
    static void AllDestroy(Passkey<GameEngine>);
    /// @brief HWNDからウィンドウインスタンスを取得
    /// @param hwnd ウィンドウハンドル
    /// @return ウィンドウインスタンスへのポインタ。存在しない場合はnullptr
    static Window *GetWindow(HWND hwnd);
    /// @brief ウィンドウタイトルから一致する最初のウィンドウインスタンスを取得
    /// @param title ウィンドウタイトル
    /// @return 一致するウィンドウインスタンスへのポインタ。存在しない場合はnullptr
    static Window *GetWindow(const std::string &title);
    /// @brief ウィンドウタイトルからウィンドウインスタンスを取得
    /// @param title ウィンドウタイトル
    /// @return 一致するウィンドウインスタンスへのポインタのリスト。存在しない場合は空のリスト
    static std::vector<Window *> GetWindows(const std::string &title);
#if defined(USE_IMGUI)
    /// @brief 存在するウィンドウのうち一番最初のHWNDを取得（ImGuiManager用）
    static HWND GetFirstWindowHwndForImGui(Passkey<ImGuiManager>);
#endif

    /// @brief 現在のウィンドウ数を取得
    static size_t GetWindowCount();

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
    /// @brief ウィンドウ描画処理
    static void Draw(Passkey<GameEngine>);
    /// @brief 破棄反映処理
    static void CommitDestroy(Passkey<GameEngine>);

    /// @brief 通常ウィンドウ生成
    /// @param title ウィンドウタイトル
    /// @param width ウィンドウ幅
    /// @param height ウィンドウ高さ
    /// @param style ウィンドウスタイル
    /// @param iconPath アイコンパス
    /// @return 生成されたウィンドウインスタンスへのポインタ
    static Window *CreateNormal(const std::string &title = "", int32_t width = 0, int32_t height = 0, DWORD style = 0, const std::string &iconPath = "");
    /// @brief オーバーレイウィンドウ生成
    /// @param title ウィンドウタイトル
    /// @param width ウィンドウ幅
    /// @param height ウィンドウ高さ
    /// @param clickThrough 通常クリック透過設定
    /// @param iconPath アイコンパス
    /// @return 生成されたウィンドウインスタンスへのポインタ
    static Window *CreateOverlay(const std::string &title = "", int32_t width = 0, int32_t height = 0, bool clickThrough = false, const std::string &iconPath = "");

    /// @brief コンストラクタ（Window限定）
    /// @param windowType ウィンドウの種類
    /// @param title ウィンドウタイトル
    /// @param width ウィンドウ幅
    /// @param height ウィンドウ高さ
    /// @param windowStyle ウィンドウスタイル
    /// @param iconPath アイコンパス
    Window(Passkey<Window>,
        WindowType windowType,
        const std::wstring &title = L"GameWindow",
        int32_t width = 1280,
        int32_t height = 720,
        DWORD windowStyle = WS_OVERLAPPEDWINDOW,
        const std::wstring &iconPath = L"");
    ~Window();

    // ウィンドウの破棄通知
    void DestroyNotify();
    /// @brief 破棄予定かどうか
    bool IsPendingDestroy() const noexcept { return isPendingDestroy_; }

    /// @brief WindowsAPIから呼ばれるイベントハンドラ
    std::optional<LRESULT> HandleEvent(Passkey<WindowsAPI>, UINT msg, WPARAM wparam, LPARAM lparam);

    /// @brief サイズ変更モードを設定する
    void SetSizeChangeMode(SizeChangeMode sizeChangeMode);
    /// @brief ウィンドウモードを設定する
    /// @param windowMode ウィンドウモード
    void SetWindowMode(WindowMode windowMode);
    /// @brief ウィンドウタイトルを設定する
    /// @param title ウィンドウタイトル
    void SetWindowTitle(const std::wstring &title);
    /// @brief ウィンドウタイトルを設定する
    /// @param title ウィンドウタイトル
    void SetWindowTitle(const std::string &title);
    /// @brief ウィンドウサイズを設定する
    /// @param width クライアント幅
    /// @param height クライアント高さ
    void SetWindowSize(int32_t width, int32_t height);
    /// @brief ウィンドウ位置を設定する
    /// @param x ウィンドウ左上のX座標
    /// @param y ウィンドウ左上のY座標
    void SetWindowPosition(int32_t x, int32_t y);
    /// @brief ウィンドウの表示/非表示を切り替える
    void SetWindowVisible(bool visible);
	/// @brief 親ウィンドウを設定する
	/// @param parentHwnd 親ウィンドウのHWND
	void SetWindowParent(HWND parentHwnd, bool applyNative = true); // 物理(SetParent)反映可否を制御するフラグを追加（デフォルト true）
    /// @brief 親ウィンドウを設定する
    /// @param parentWindow 親ウィンドウのポインタ
    void SetWindowParent(Window *parentWindow, bool applyNative = true); // 物理(SetParent)反映可否を制御するフラグを追加（デフォルト true）
    /// @brief 子ウィンドウを設定する
    /// @param childHwnd 子ウィンドウのHWND
    void SetWindowChild(HWND childHwnd, bool applyNative = true); // 物理(SetParent)反映可否を制御するフラグを追加（デフォルト true）
    /// @brief 子ウィンドウを設定する
    /// @param childWindow 子ウィンドウのポインタ
    void SetWindowChild(Window *childWindow, bool applyNative = true); // 物理(SetParent)反映可否を制御するフラグを追加（デフォルト true）
    /// @brief 親ウィンドウを解除する
    void ClearWindowParent(bool applyNative = true); // 親解除
    /// @brief 子ウィンドウを解除する
    void ClearWindowChild(bool applyNative = true);  // 全子解除

    /// @brief ウィンドウイベントを登録する（既定イベント型は値で、拡張イベントはunique_ptrで保持）
    template<class TEvent, class... Args>
    requires (IsDefaultEventV<TEvent>)
    void RegisterWindowEvent(Args&&... args) {
        // 一度だけ仮生成してメッセージ値を取得
        TEvent temp(std::forward<Args>(args)...);
        const UINT msg = temp.kTargetMessage_;
        // variant を in-place 構築（コピー/ムーブ不要）
        auto &slot = eventHandlers_[msg];
        slot.template emplace<TEvent>(std::move(temp));
        // Window を紐付け
        std::get<TEvent>(slot).SetWindow(this);
    }

    /// @brief ウィンドウ既定イベントを登録する（unique_ptr版、値にムーブして保持）
    template<class TEvent>
    requires (IsDefaultEventV<TEvent>)
    void RegisterWindowEvent(std::unique_ptr<TEvent> handler) {
        if (!handler) return;
        const UINT msg = handler->kTargetMessage_;
        auto &slot = eventHandlers_[msg];
        slot.template emplace<TEvent>(std::move(*handler));
        std::get<TEvent>(slot).SetWindow(this);
    }

    /// @brief ウィンドウ拡張イベントを登録する（unique_ptr保持）
    template<class TEvent> requires (IsUserEventV<TEvent>)
    void RegisterWindowEvent(std::unique_ptr<TEvent> handler) {
        if (!handler) return;
        handler->SetWindow(this);
        const UINT msg = handler->kTargetMessage_;
        // 基底型のunique_ptrに変換して格納（releaseで安全に移管）
        std::unique_ptr<IWindowEvent> basePtr(handler.release());
        auto &slot = eventHandlers_[msg];
        slot.template emplace<std::unique_ptr<IWindowEvent>>(std::move(basePtr));
    }

    /// @brief ウィンドウ拡張イベントを登録する（値指定でも内部でunique_ptr化）
    template<class TEvent, class... Args> requires (IsUserEventV<TEvent>)
    void RegisterWindowEvent(Args&&... args) {
        auto ptr = std::make_unique<TEvent>(std::forward<Args>(args)...);
        ptr->SetWindow(this);
        const UINT msg = ptr->kTargetMessage_;
        std::unique_ptr<IWindowEvent> basePtr(ptr.release());
        auto &slot = eventHandlers_[msg];
        slot.template emplace<std::unique_ptr<IWindowEvent>>(std::move(basePtr));
    }

    /// @brief ウィンドウイベントの登録解除
    void UnregisterWindowEvent(UINT msg);

    /// @brief ウィンドウの種類を取得する
    WindowType GetWindowType() const noexcept { return windowType_; }
    /// @brief ウィンドウモードを取得する
    WindowMode GetWindowMode() const noexcept { return windowMode_; }
    /// @brief サイズ変更モードを取得する
    SizeChangeMode GetSizeChangeMode() const noexcept { return sizeChangeMode_; }
    /// @brief ウィンドウタイトルを取得する
    const std::string &GetWindowTitle() const noexcept { return descriptor_.title; }
    /// @brief ウィンドウハンドルを取得する
    HWND GetWindowHandle() const noexcept { return descriptor_.hwnd; }
    /// @brief ウィンドウクラスを取得する
    const WNDCLASS& GetWindowClass() const noexcept { return descriptor_.wc; }
    /// @brief ウィンドウ位置（左上座標）を取得する
    POINT GetWindowPosition() const noexcept { return { size_.windowRect.left, size_.windowRect.top }; }
    /// @brief クライアント幅を取得する
    int32_t GetClientWidth() const noexcept { return size_.clientWidth; }
    /// @brief クライアント高さを取得する
    int32_t GetClientHeight() const noexcept { return size_.clientHeight; }
    /// @brief アスペクト比を取得する
    float GetAspectRatio() const noexcept { return size_.aspectRatio; }
    /// @brief コマンドリストを取得する
    ID3D12GraphicsCommandList *GetCommandList() const;

    /// @brief 指定のウィンドウスタイルを持っているかどうかをチェック
    bool HasWindowStyle(DWORD style) const noexcept { return (descriptor_.windowStyle & style) != 0; }
    /// @brief 指定のメッセージが来ているかどうかをチェック
    bool HasMessage(UINT msg) const { return messages_.find(msg) != messages_.end(); }
    /// @brief 指定のメッセージの情報を取得
    const WindowMessage &GetWindowMessage(UINT msg) const;

    // 親子取得
    Window *GetParentWindow() const noexcept { return parentWindow_; }
    const std::vector<Window *> &GetChildWindows() const noexcept { return childWindows_; }

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
    /// @brief ウィンドウが表示されているかどうかを取得する
    bool IsVisible() const noexcept { return IsWindowVisible(descriptor_.hwnd) != FALSE; }

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
    /// @param windowType ウィンドウの種類
    /// @param title ウィンドウタイトル
    /// @param width ウィンドウ幅
    /// @param height ウィンドウ高さ
    /// @param windowStyle ウィンドウスタイル
    /// @param iconPath アイコンのパス
    /// @return 初期化成功かどうか
    bool InitializeWindow(
        WNDPROC windowProc,
        WindowType windowType,
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

    // 親子関係管理ヘルパー
    void DetachFromParentUnsafe(bool applyNative = true);
    void DetachAllChildrenUnsafe(bool applyNative = true);
    void RemoveChildPointerUnsafe(Window *child);

    // ウィンドウ関連
    WindowDescriptor descriptor_{};
    // サイズ関連
    WindowSize size_{};
    // メッセージ関連
    std::unordered_map<UINT, WindowMessage> messages_;
    
    // DX12スワップチェーン
    DX12SwapChain *dx12SwapChain_ = nullptr;

    // 状態管理
    WindowType windowType_ = WindowType::Normal;
    WindowMode windowMode_ = WindowMode::Window;
    SizeChangeMode sizeChangeMode_ = SizeChangeMode::Normal;

    // 内部で保持するワイド文字列（WinAPIのクラス名/タイトル用）
    std::wstring titleW_ = L"";

    // イベントハンドラマップ（既定イベント or ユーザーイベント）
    std::unordered_map<UINT, Events> eventHandlers_;

    // 親子管理データ
    Window *parentWindow_ = nullptr;
    std::vector<Window*> childWindows_{};

    // 破棄通知フラグ
    bool isPendingDestroy_ = false;
};

} // namespace KashipanEngine