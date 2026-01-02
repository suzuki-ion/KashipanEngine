#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>

#include <windows.h>

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

    /// @brief 指定マウスボタンが押されているかを取得
    bool IsButtonDown(int button) const;
    /// @brief 指定マウスボタンが押された瞬間か（トリガー）を取得
    bool IsButtonTrigger(int button) const;
    /// @brief 指定マウスボタンが離された瞬間か（リリース）を取得
    bool IsButtonRelease(int button) const;
    /// @brief 前フレームで指定マウスボタンが押されていたかを取得
    bool WasButtonDown(int button) const;

    /// @brief マウス移動量（X方向、フレーム差分）を取得
    int GetDeltaX() const;
    /// @brief マウス移動量（Y方向、フレーム差分）を取得
    int GetDeltaY() const;

    /// @brief マウスホイールのフレーム差分（縦方向）を取得
    int GetWheel() const;

    /// @brief マウスホイールの累積値（縦方向）を取得
    int GetWheelValue() const;

    /// @brief 前フレームのマウス移動量（X方向、フレーム差分）を取得
    int GetPrevDeltaX() const;
    /// @brief 前フレームのマウス移動量（Y方向、フレーム差分）を取得
    int GetPrevDeltaY() const;

    /// @brief 前フレームのマウスホイール差分（縦方向）を取得
    int GetPrevWheel() const;

    /// @brief 前フレームのマウスホイール累積値（縦方向）を取得
    int GetPrevWheelValue() const;

    /// @brief 指定ウィンドウのクライアント座標系でのマウス座標を取得
    POINT GetPos(HWND hwnd) const;
    /// @brief 指定ウィンドウの前フレームのクライアント座標系でのマウス座標を取得
    POINT GetPrevPos(HWND hwnd) const;
    /// @brief 指定ウィンドウのクライアント座標（X）を取得
    int GetX(HWND hwnd) const;
    /// @brief 指定ウィンドウのクライアント座標（Y）を取得
    int GetY(HWND hwnd) const;
    /// @brief 指定ウィンドウの前フレームのクライアント座標（X）を取得
    int GetPrevX(HWND hwnd) const;
    /// @brief 指定ウィンドウの前フレームのクライアント座標（Y）を取得
    int GetPrevY(HWND hwnd) const;

    /// @brief 指定ウィンドウのクライアント座標系でのマウス座標を取得
    POINT GetPos(const Window* window) const;
    /// @brief 指定ウィンドウの前フレームのクライアント座標系でのマウス座標を取得
    POINT GetPrevPos(const Window* window) const;
    /// @brief 指定ウィンドウのクライアント座標（X）を取得
    int GetX(const Window* window) const;
    /// @brief 指定ウィンドウのクライアント座標（Y）を取得
    int GetY(const Window* window) const;
    /// @brief 指定ウィンドウの前フレームのクライアント座標（X）を取得
    int GetPrevX(const Window* window) const;
    /// @brief 指定ウィンドウの前フレームのクライアント座標（Y）を取得
    int GetPrevY(const Window* window) const;

private:
    static std::uintptr_t GetWindowKey_(HWND hwnd) noexcept {
        return reinterpret_cast<std::uintptr_t>(hwnd);
    }

    std::array<std::uint8_t, 8> currentButtons_{};
    std::array<std::uint8_t, 8> previousButtons_{};

    int currentDeltaX_ = 0;
    int currentDeltaY_ = 0;
    int currentWheel_ = 0;
    int currentWheelValue_ = 0;

    int previousDeltaX_ = 0;
    int previousDeltaY_ = 0;
    int previousWheel_ = 0;
    int previousWheelValue_ = 0;

    POINT currentPosScreen{};
    POINT previousPosScreen{};

    mutable std::unordered_map<std::uintptr_t, POINT> prevClientPosByWindow_;

    bool initialized_ = false;
};

} // namespace KashipanEngine
