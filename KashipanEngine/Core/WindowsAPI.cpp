#include "WindowsAPI.h"
#include <cassert>
#include <memory>
#include <unordered_map>
#include "Utilities/Conversion/ConvertString.h"
#include "Debug/Logger.h"

namespace KashipanEngine {

namespace {
// ウィンドウインスタンスのマップ
std::unordered_map<HWND, std::unique_ptr<Window>> sWindowMap;
} // namespace

LRESULT CALLBACK WindowsAPI::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    LogScope scope;
    // ウィンドウインスタンスを取得
    auto it = sWindowMap.find(hwnd);
    if (it == sWindowMap.end()) {
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
    Window *window = it->second.get();
    assert(window && "Window instance is null");
    // ウィンドウインスタンスにイベントを処理させる
    auto result = window->HandleEvent(Passkey<WindowsAPI>{}, msg, wparam, lparam);
    if (result.has_value()) {
        return result.value();
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

WindowsAPI::WindowsAPI(Passkey<GameEngine>, const std::string &defaultTitle,
    int32_t defaultWidth, int32_t defaultHeight, DWORD defaultWindowStyle,
    const std::string &defaultIconPath) {
    LogScope scope;
    defaultTitle_ = defaultTitle;
    defaultWidth_ = defaultWidth;
    defaultHeight_ = defaultHeight;
    defaultWindowStyle_ = defaultWindowStyle;
    defaultIconPath_ = defaultIconPath;
}

WindowsAPI::~WindowsAPI() {
    LogScope scope;
    // すべてのウィンドウを破棄
    sWindowMap.clear();
}

void WindowsAPI::Update(Passkey<GameEngine>) {
    LogScope scope;
    for (auto &pair : sWindowMap) {
        pair.second->ClearMessages({});
        pair.second->Update({});
    }
}

Window *WindowsAPI::CreateWindowInstance(const std::string &title,
    int32_t width, int32_t height, DWORD style,
    const std::string &iconPath) {
    LogScope scope;
    // パラメータの設定
    std::string windowTitle = title.empty() ? defaultTitle_ : title;
    int32_t windowWidth = (width <= 0) ? defaultWidth_ : width;
    int32_t windowHeight = (height <= 0) ? defaultHeight_ : height;
    DWORD windowStyle = (style == 0) ? defaultWindowStyle_ : style;
    std::string windowIconPath = iconPath.empty() ? defaultIconPath_ : iconPath;

    // ウィンドウインスタンスの作成
    std::unique_ptr<Window> window = std::make_unique<Window>(
        Passkey<WindowsAPI>{},
        WindowProc,
        ConvertString(windowTitle),
        windowWidth,
        windowHeight,
        windowStyle,
        ConvertString(windowIconPath)
    );
    if (!window->GetWindowHandle()) {
        return nullptr;
    }

    // ウィンドウマップに登録
    HWND hwnd = window->GetWindowHandle();
    sWindowMap[hwnd] = std::move(window);
    return sWindowMap[hwnd].get();
}

bool WindowsAPI::DestroyWindowInstance(Window *window) {
    LogScope scope;
    if (!window) {
        return false;
    }
    HWND hwnd = window->GetWindowHandle();
    if (sWindowMap.find(hwnd) != sWindowMap.end()) {
        sWindowMap.erase(hwnd);
        return true;
    }
    return false;
}

} // namespace KashipanEngine