#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

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

/// @brief CrashHandler用パスキー
class PasskeyForCrashHandler final {
    friend LONG WINAPI CrashHandler(EXCEPTION_POINTERS *exceptionInfo);
    PasskeyForCrashHandler() = default;
};

} // namespace KashipanEngine