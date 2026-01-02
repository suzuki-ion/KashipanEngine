#pragma once
#include <windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <cstdint>
#include <unordered_map>

namespace KashipanEngine {

class Window;

class Mouse {
public:
    Mouse();
    ~Mouse();

    Mouse(const Mouse&) = delete;
    Mouse(Mouse&&) = delete;
    Mouse& operator=(const Mouse&) = delete;
    Mouse& operator=(Mouse&&) = delete;

    void Initialize(HINSTANCE hInstance);
    void Finalize();
    void Update();

    /// @brief 指定ボタンが押されているかを取得
    /// @param button ボタン番号（0-7）
    /// @return 押されている場合 true
    bool IsButtonDown(int button) const;
    /// @brief 指定ボタンが押された瞬間か（トリガー）を取得
    /// @param button ボタン番号（0-7）
    /// @return 今フレームで押された場合 true
    bool IsButtonTrigger(int button) const;
    /// @brief 指定ボタンが離された瞬間か（リリース）を取得
    /// @param button ボタン番号（0-7）
    /// @return 今フレームで離された場合 true
    bool IsButtonRelease(int button) const;
    /// @brief 前フレームで指定ボタンが押されていたかを取得
    /// @param button ボタン番号（0-7）
    /// @return 前フレームで押されていた場合 true
    bool WasButtonDown(int button) const;

    /// @brief X方向移動量（デバイス報告値）を取得
    /// @return X方向移動量
    int GetDeltaX() const;
    /// @brief Y方向移動量（デバイス報告値）を取得
    /// @return Y方向移動量
    int GetDeltaY() const;
    /// @brief ホイール移動量（デバイス報告値）を取得
    /// @return ホイール移動量
    int GetWheel() const;

    /// @brief 前フレームのX方向移動量（デバイス報告値）を取得
    /// @return 前フレームのX方向移動量
    int GetPrevDeltaX() const;
    /// @brief 前フレームのY方向移動量（デバイス報告値）を取得
    /// @return 前フレームのY方向移動量
    int GetPrevDeltaY() const;
    /// @brief 前フレームのホイール移動量（デバイス報告値）を取得
    /// @return 前フレームのホイール移動量
    int GetPrevWheel() const;

    /// @brief 指定ウィンドウのクライアント座標系での現在マウス座標を取得
    /// @param hwnd 基準にするウィンドウハンドル
    /// @return クライアント座標
    POINT GetPos(HWND hwnd) const;
    /// @brief 指定ウィンドウのクライアント座標系での前回マウス座標を取得
    /// @param hwnd 基準にするウィンドウハンドル
    /// @return 前回のクライアント座標
    POINT GetPrevPos(HWND hwnd) const;
    /// @brief 指定ウィンドウ基準でのX座標を取得
    /// @param hwnd 基準にするウィンドウハンドル
    /// @return X座標
    int GetX(HWND hwnd) const;
    /// @brief 指定ウィンドウ基準でのY座標を取得
    /// @param hwnd 基準にするウィンドウハンドル
    /// @return Y座標
    int GetY(HWND hwnd) const;
    /// @brief 指定ウィンドウ基準での前回X座標を取得
    /// @param hwnd 基準にするウィンドウハンドル
    /// @return 前回X座標
    int GetPrevX(HWND hwnd) const;
    /// @brief 指定ウィンドウ基準での前回Y座標を取得
    /// @param hwnd 基準にするウィンドウハンドル
    /// @return 前回Y座標
    int GetPrevY(HWND hwnd) const;

    /// @brief 指定ウィンドウのクライアント座標系での現在マウス座標を取得
    /// @param window 基準にするウィンドウ
    /// @return クライアント座標
    POINT GetPos(const Window* window) const;
    /// @brief 指定ウィンドウのクライアント座標系での前回マウス座標を取得
    /// @param window 基準にするウィンドウ
    /// @return 前回のクライアント座標
    POINT GetPrevPos(const Window* window) const;
    /// @brief 指定ウィンドウ基準でのX座標を取得
    /// @param window 基準にするウィンドウ
    /// @return X座標
    int GetX(const Window* window) const;
    /// @brief 指定ウィンドウ基準でのY座標を取得
    /// @param window 基準にするウィンドウ
    /// @return Y座標
    int GetY(const Window* window) const;
    /// @brief 指定ウィンドウ基準での前回X座標を取得
    /// @param window 基準にするウィンドウ
    /// @return 前回X座標
    int GetPrevX(const Window* window) const;
    /// @brief 指定ウィンドウ基準での前回Y座標を取得
    /// @param window 基準にするウィンドウ
    /// @return 前回Y座標
    int GetPrevY(const Window* window) const;

private:
    static std::uintptr_t GetWindowKey_(HWND hwnd) noexcept {
        return reinterpret_cast<std::uintptr_t>(hwnd);
    }

    DIMOUSESTATE currentState{};
    DIMOUSESTATE previousState{};

    // スクリーン座標（デスクトップ全体）
    POINT currentPosScreen{};
    POINT previousPosScreen{};

    // ウィンドウごとの「前回クライアント座標」
    mutable std::unordered_map<std::uintptr_t, POINT> prevClientPosByWindow_;
};

} // namespace KashipanEngine
