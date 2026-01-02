#pragma once

#include <array>
#include <cstdint>

#include <windows.h>

#include "Input/Key.h"

namespace KashipanEngine {

class Keyboard {
public:
    Keyboard();
    ~Keyboard();

    Keyboard(const Keyboard&) = delete;
    Keyboard(Keyboard&&) = delete;
    Keyboard& operator=(const Keyboard&) = delete;
    Keyboard& operator=(Keyboard&&) = delete;

    void Initialize();
    void Finalize();
    void Update();

    /// @brief 指定キーが押されているかを取得
    bool IsDown(Key key) const;
    /// @brief 指定キーが押された瞬間か（トリガー）を取得
    bool IsTrigger(Key key) const;
    /// @brief 指定キーが離された瞬間か（リリース）を取得
    bool IsRelease(Key key) const;
    /// @brief 前フレームで指定キーが押されていたかを取得
    bool WasDown(Key key) const;

private:
    static size_t ToIndex_(Key key) noexcept;

    std::array<std::uint8_t, 256> current{};
    std::array<std::uint8_t, 256> previous{};

    bool initialized_ = false;
};

} // namespace KashipanEngine
