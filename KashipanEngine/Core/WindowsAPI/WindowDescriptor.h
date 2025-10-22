#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

namespace KashipanEngine {

struct WindowDescriptor {
    std::string title;          // ウィンドウタイトル
    WNDCLASS wc{};              // ウィンドウクラス
    HWND hwnd{};                // ウィンドウハンドル
    HINSTANCE hInstance{};      // インスタンスハンドル
    DWORD windowStyle{};        // ウィンドウスタイル
};

} // namespace KashipanEngine