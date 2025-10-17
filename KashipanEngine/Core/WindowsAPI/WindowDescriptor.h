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
    bool isVisible = false;     // ウィンドウが表示されているか
    bool isActive = false;      // ウィンドウがアクティブか
    bool isMinimized = false;   // ウィンドウが最小化されているか
    bool isMaximized = false;   // ウィンドウが最大化されているか
};

} // namespace KashipanEngine