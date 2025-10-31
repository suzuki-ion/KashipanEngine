#include "WindowsAPI.h"
#include <cassert>
#include <memory>
#include <unordered_map>
#include "Utilities/Conversion/ConvertString.h"
#include "Debug/Logger.h"

namespace KashipanEngine {

namespace {
// ウィンドウインスタンスのマップ
std::unordered_map<HWND, Window*> sWindowMap;
} // namespace

LRESULT CALLBACK WindowsAPI::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    LogScope scope;
    // ウィンドウインスタンスを取得
    auto it = sWindowMap.find(hwnd);
    if (it == sWindowMap.end()) {
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
    Window *window = it->second;
    assert(window && "Window instance is null");
    // ウィンドウインスタンスにイベントを処理させる
    auto result = window->HandleEvent(Passkey<WindowsAPI>{}, msg, wparam, lparam);
    if (result.has_value()) {
        return result.value();
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

WindowsAPI::WindowsAPI(Passkey<GameEngine>) {}

WindowsAPI::~WindowsAPI() {
    LogScope scope;
}

bool WindowsAPI::RegisterWindow(Passkey<Window>, Window *window) {
    LogScope scope;
    assert(window && "Window instance is null");
    HWND hwnd = window->GetWindowHandle();
    if (sWindowMap.find(hwnd) != sWindowMap.end()) {
        Log("Window is already registered.", LogSeverity::Warning);
        return false;
    }
    sWindowMap[hwnd] = window;
    return true;
}

bool WindowsAPI::UnregisterWindow(Passkey<Window>, HWND hwnd) {
    LogScope scope;
    auto it = sWindowMap.find(hwnd);
    if (it == sWindowMap.end()) {
        Log("Window not found for unregistration.", LogSeverity::Warning);
        return false;
    }
    sWindowMap.erase(it);
    return true;
}

} // namespace KashipanEngine