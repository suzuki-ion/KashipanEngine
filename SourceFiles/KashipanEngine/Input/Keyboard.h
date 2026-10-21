#pragma once

#include <array>
#include <cstdint>

#include <windows.h>

#include "Input/Key.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class Input;

class Keyboard {
public:
    Keyboard(Passkey<Input>);
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

    /// @brief いずれかのキーが押された瞬間か（トリガー）を取得
    /// @details `Key` に対応していないキー（記号キー等）も含めて判定する
    bool IsAnyTrigger() const;

private:
    static size_t ToIndex_(Key key) noexcept;

    std::array<std::uint8_t, 256> current{};
    std::array<std::uint8_t, 256> previous{};

    // `Key` への変換可否に関わらず押されているキーの数（入力デバイス判定用）
    std::uint32_t rawDownCount_ = 0;
    std::uint32_t prevRawDownCount_ = 0;

    bool initialized_ = false;
};

} // namespace KashipanEngine
