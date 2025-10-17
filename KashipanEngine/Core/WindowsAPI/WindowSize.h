#pragma once
#include <windows.h>
#include <string>

namespace KashipanEngine {

struct WindowSize {
    int32_t clientWidth = 0;    // クライアント領域の幅
    int32_t clientHeight = 0;   // クライアント領域の高さ
    float aspectRatio = 0.0f;   // アスペクト比
    RECT windowRect{};          // ウィンドウ矩形
    RECT clientRect{};          // クライアント矩形
};

} // namespace KashipanEngine