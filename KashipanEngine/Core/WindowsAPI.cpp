#include "WindowsAPI.h"
#include <cassert>
#include <memory>
#include <unordered_map>
#include "Utilities/Conversion/ConvertString.h"

namespace KashipanEngine {

namespace {
// ウィンドウインスタンスのマップ
std::unordered_map<HWND, std::unique_ptr<Window>> sWindowMap;
} // namespace

std::unique_ptr<WindowsAPI> WindowsAPI::Factory::Create(const std::string &defaultTitle, int32_t defaultWidth, int32_t defaultHeight, DWORD defaultWindowStyle, const std::string &defaultIconPath) {
    return std::unique_ptr<WindowsAPI>(new WindowsAPI(defaultTitle, defaultWidth, defaultHeight, defaultWindowStyle, defaultIconPath));
}

WindowsAPI::~WindowsAPI() {
    // すべてのウィンドウを破棄
    sWindowMap.clear();
}

Window *WindowsAPI::CreateWindowInstance(const std::string &title,
    int32_t width, int32_t height, DWORD style,
    const std::string &iconPath) {
    // パラメータの設定
    std::string windowTitle = title.empty() ? defaultTitle_ : title;
    int32_t windowWidth = (width <= 0) ? defaultWidth_ : width;
    int32_t windowHeight = (height <= 0) ? defaultHeight_ : height;
    DWORD windowStyle = (style == 0) ? defaultWindowStyle_ : style;
    std::string windowIconPath = iconPath.empty() ? defaultIconPath_ : iconPath;

    // ウィンドウインスタンスの作成
    std::unique_ptr<Window> window = Window::Factory().Create(
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

LRESULT CALLBACK WindowsAPI::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // hwndからWindowインスタンスを取得（WM_NCCREATE前はGWLP_USERDATA未設定のためCREATESTRUCT経由）
    Window *window = nullptr;

    if (msg == WM_NCCREATE) {
        auto *createStruct = reinterpret_cast<CREATESTRUCT *>(lparam);
        window = reinterpret_cast<Window *>(createStruct->lpCreateParams);
        if (window) {
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        }
    } else {
        window = reinterpret_cast<Window *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!window) {
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }

    // Windowにディスパッチ
    if (auto handled = Window::ProcedureHandler::HandleWindowEvent(hwnd, msg, wparam, lparam); handled.has_value()) {
        return *handled;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

WindowsAPI::WindowsAPI(const std::string &defaultTitle,
    int32_t defaultWidth, int32_t defaultHeight, DWORD defaultWindowStyle,
    const std::string &defaultIconPath) {
    defaultTitle_ = defaultTitle;
    defaultWidth_ = defaultWidth;
    defaultHeight_ = defaultHeight;
    defaultWindowStyle_ = defaultWindowStyle;
    defaultIconPath_ = defaultIconPath;
}

} // namespace KashipanEngine