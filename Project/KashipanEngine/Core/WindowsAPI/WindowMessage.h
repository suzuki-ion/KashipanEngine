#pragma once
#include <windows.h>
#include <cstdint>

namespace KashipanEngine {

/// @brief ウィンドウメッセージ情報構造体
struct WindowMessage {
    UINT msg;
    WPARAM wparam;
    LPARAM lparam;
};

} // namespace KashipanEngine