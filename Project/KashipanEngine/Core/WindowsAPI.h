#pragma once
#include <Windows.h>
#include <string>
#include <memory>
#include <vector>
#include <optional>

namespace KashipanEngine {

class GameEngine;
class Window;
class WindowsAPI;

/// @brief モニター個々の情報を保持する構造体
/// @details 取得と利用はプロパティ風ゲッターを使用してください。
struct MonitorInfo final {
    constexpr MonitorInfo() = default;

    /// @brief モニターハンドル取得
    HMONITOR MonitorHandle() const noexcept { return monitor_; }
    /// @brief モニター全体の矩形取得（ピクセル）
    const RECT &MonitorRect() const noexcept { return monitorRect_; }
    /// @brief 作業領域取得（タスクバー等を除く）
    const RECT &WorkArea() const noexcept { return workArea_; }
    /// @brief タスクバーの矩形取得（プライマリ/セカンダリ問わず該当するもの。見つからない場合はゼロ）
    const RECT &TaskbarRect() const noexcept { return taskbarRect_; }
    /// @brief タスクバーの辺取得（ABE_LEFT/RIGHT/TOP/BOTTOM）。未設定は UINT -1
    UINT TaskbarEdge() const noexcept { return taskbarEdge_; }
    /// @brief 解像度取得（ピクセル）
    int32_t Width() const noexcept { return width_; }
    /// @brief 解像度取得（ピクセル）
    int32_t Height() const noexcept { return height_; }
    /// @brief DPI取得（Per-Monitor DPI 対応）
    UINT DpiX() const noexcept { return dpiX_; }
    /// @brief DPI取得（Per-Monitor DPI 対応）
    UINT DpiY() const noexcept { return dpiY_; }
    /// @brief DPIスケール（100% = 1.0）
    float DpiScaleX() const noexcept { return dpiScaleX_; }
    /// @brief DPIスケール（100% = 1.0）
    float DpiScaleY() const noexcept { return dpiScaleY_; }
    /// @brief リフレッシュレート取得（FPS）。不明時は 0.0f
    float FPS() const noexcept { return fps_; }

private:
    friend class WindowsAPI;

    // モニター識別用
    HMONITOR monitor_ = nullptr;
    // モニター全体の矩形（ピクセル）
    RECT monitorRect_{ 0, 0, 0, 0 };
    // 作業領域（タスクバー等を除く）
    RECT workArea_{ 0, 0, 0, 0 };
    // タスクバーの矩形（プライマリ/セカンダリ問わず該当するもの。見つからない場合はゼロ）
    RECT taskbarRect_{ 0, 0, 0, 0 };
    // タスクバーの辺（ABE_LEFT/RIGHT/TOP/BOTTOM）。未設定は UINT -1
    UINT taskbarEdge_ = static_cast<UINT>(-1);
    // 解像度（ピクセル）
    int32_t width_ = 0;
    int32_t height_ = 0;
    // DPI（Per-Monitor DPI 対応）
    UINT dpiX_ = 96;
    UINT dpiY_ = 96;
    // DPIスケーリング率（100%基準）
    float dpiScaleX_ = 1.0f;
    float dpiScaleY_ = 1.0f;
    // リフレッシュレート（FPS）。不明時は 0.0f
    float fps_ = 0.0f;
};

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

    /// @brief 指定モニターの情報取得（失敗時は nullopt）
    /// @param monitor 対象モニターのハンドル（nullptr可: 最も近いプライマリを返す）
    static std::optional<MonitorInfo> QueryMonitorInfo(HMONITOR monitor = nullptr);

    /// @brief 接続されている全モニター情報取得
    static std::vector<MonitorInfo> QueryAllMonitorInfos();

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