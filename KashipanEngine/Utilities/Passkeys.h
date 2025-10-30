#pragma once
#include <string>
#include <Windows.h>

namespace KashipanEngine {

/// @brief パスキー
/// @tparam T パスキーを発行できるクラス
template <typename T>
class Passkey final {
    friend T;
    Passkey() = default;
};

/// @brief WinMain用パスキー
class PasskeyForWinMain final {
    friend int ::WinMain(HINSTANCE, HINSTANCE, LPSTR, int);
    PasskeyForWinMain() = default;
};

/// @brief GameEngineMain用パスキー
class PasskeyForGameEngineMain final {
    friend int Execute(PasskeyForWinMain, const std::string &);
    PasskeyForGameEngineMain() = default;
};

} // namespace KashipanEngine