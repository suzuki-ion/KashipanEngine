#pragma once

#include <cstdint>
#include <array>

#include <windows.h>

namespace KashipanEngine {

class Keyboard {
public:
    Keyboard();
    ~Keyboard();

    Keyboard(const Keyboard&) = delete;
    Keyboard(Keyboard&&) = delete;
    Keyboard& operator=(const Keyboard&) = delete;
    Keyboard& operator=(Keyboard&&) = delete;

    void Initialize(HINSTANCE hInstance, HWND hwnd);
    void Finalize();
    void Update();

    /// @brief 指定キーが押されているかを取得
    /// @param key 仮想キーコード（VK_*）
    /// @return 押されている場合 true
    bool IsDown(int key) const;
    /// @brief 指定キーが押された瞬間か（トリガー）を取得
    /// @param key 仮想キーコード（VK_*）
    /// @return 今フレームで押された場合 true
    bool IsTrigger(int key) const;
    /// @brief 指定キーが離された瞬間か（リリース）を取得
    /// @param key 仮想キーコード（VK_*）
    /// @return 今フレームで離された場合 true
    bool IsRelease(int key) const;
    /// @brief 前フレームで指定キーが押されていたかを取得
    /// @param key 仮想キーコード（VK_*）
    /// @return 前フレームで押されていた場合 true
    bool WasDown(int key) const;

private:
    std::array<std::uint8_t, 256> current_{};
    std::array<std::uint8_t, 256> previous_{};
};

} // namespace KashipanEngine
