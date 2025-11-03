#include "Window.h"
#include "Core/WindowsAPI.h"
#include "Utilities/Conversion/ConvertString.h"
#include <cassert>

#include "Core/WindowsAPI/WindowEvents/DefaultEvents/DestroyEvent.h"
#include "Core/WindowsAPI/WindowEvents/DefaultEvents/CloseEvent.h"
#include "Core/WindowsAPI/WindowEvents/DefaultEvents/SizeEvent.h"
#include "Core/WindowsAPI/WindowEvents/DefaultEvents/EnterSizeMoveEvent.h"
#include "Core/WindowsAPI/WindowEvents/DefaultEvents/ExitSizeMoveEvent.h"
#include "Core/WindowsAPI/WindowEvents/DefaultEvents/ActivateEvent.h"
#include "Core/WindowsAPI/WindowEvents/DefaultEvents/GetMinMaxInfoEvent.h"
#include "Core/WindowsAPI/WindowEvents/DefaultEvents/SizingEvent.h"

namespace KashipanEngine {

namespace {
/// @brief ウィンドウ管理用マップ
std::unordered_map<HWND, std::unique_ptr<Window>> sWindowMap;
} // namespace

Window::~Window() {
    LogScope scope;
}

void Window::SetDefaultParams(Passkey<GameEngine>, const std::string &title, int32_t width, int32_t height, DWORD style, const std::string &iconPath) {
    LogScope scope;
    windowDefaultTitle = title;
    windowDefaultWidth = width;
    windowDefaultHeight = height;
    windowDefaultStyle = style;
    windowDefaultIconPath = iconPath;
}

void Window::AllDestroy(Passkey<GameEngine>) {
    LogScope scope;
    for (auto &pair : sWindowMap) {
        pair.second->Cleanup();
    }
    sWindowMap.clear();
}

Window *Window::GetWindow(HWND hwnd) {
    auto it = sWindowMap.find(hwnd);
    if (it != sWindowMap.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<Window *> Window::GetWindows(const std::string &title) {
    std::vector<Window *> windows;
    for (auto &pair : sWindowMap) {
        if (pair.second->descriptor_.title == title) {
            windows.push_back(pair.second.get());
        }
    }
    return windows;
}

bool Window::IsExist(HWND hwnd) {
    auto it = sWindowMap.find(hwnd);
    if (it != sWindowMap.end()) {
        return true;
    }
    return false;
}

bool Window::IsExist(Window *window) {
    for (auto &pair : sWindowMap) {
        if (pair.second.get() == window) {
            return true;
        }
    }
    return false;
}

bool Window::IsExist(const std::string &title) {
    for (auto &pair : sWindowMap) {
        if (pair.second->descriptor_.title == title) {
            return true;
        }
    }
    return false;
}

void Window::Update(Passkey<GameEngine>) {
    LogScope scope;
    for (auto it = sWindowMap.begin(); it != sWindowMap.end();) {
        Window *window = it->second.get();
        window->ClearMessages();
        window->ProcessMessage();
        if (window->IsDestroyed()) {
            sWindowsAPI->UnregisterWindow({}, window->GetWindowHandle());
            it = sWindowMap.erase(it);
        } else {
            ++it;
        }
    }
}

Window::Window(Passkey<Window>,
    const std::wstring &title,
    int32_t width,
    int32_t height,
    DWORD windowStyle,
    const std::wstring &iconPath) {
    LogScope scope;
    // ウィンドウの初期化を実行
    bool result = InitializeWindow(sWindowsAPI->WindowProc, title, width, height, windowStyle, iconPath);
    assert(result && "Window initialization failed");
    messages_.reserve(kMaxMessages);
    eventHandlers_.reserve(kMaxMessages);

    using namespace WindowEventDefault;
    // 標準イベントを登録
    RegisterWindowEvent(std::make_unique<DestroyEvent>());
    RegisterWindowEvent(std::make_unique<CloseEvent>());
    RegisterWindowEvent(std::make_unique<SizeEvent>());
    RegisterWindowEvent(std::make_unique<EnterSizeMoveEvent>());
    RegisterWindowEvent(std::make_unique<ExitSizeMoveEvent>());
    RegisterWindowEvent(std::make_unique<ActivateEvent>());
    RegisterWindowEvent(std::make_unique<GetMinMaxInfoEvent>());
    RegisterWindowEvent(std::make_unique<SizingEvent>());
}

Window *Window::Create(const std::string &title, int32_t width, int32_t height, DWORD style, const std::string &iconPath) {
    LogScope scope;

    std::wstring windowTitle = title.empty() ? ConvertString(windowDefaultTitle) : ConvertString(title);
    int32_t windowWidth = (width <= 0) ? windowDefaultWidth : width;
    int32_t windowHeight = (height <= 0) ? windowDefaultHeight : height;
    DWORD windowStyle = (style == 0) ? WS_OVERLAPPEDWINDOW : style;
    std::wstring windowIconPath = iconPath.empty() ? ConvertString(windowDefaultIconPath) : ConvertString(iconPath);
    
    auto window = std::make_unique<Window>(Passkey<Window>{}, windowTitle, windowWidth, windowHeight, windowStyle, windowIconPath);
    HWND hwnd = window->GetWindowHandle();

    sWindowMap[hwnd] = std::move(window);
    sWindowsAPI->RegisterWindow({}, sWindowMap[hwnd].get());
    return sWindowMap[hwnd].get();
}

void Window::Destroy() {
    LogScope scope;
    if (descriptor_.hwnd) {
        Cleanup();
    }
}

std::optional<LRESULT> Window::HandleEvent(Passkey<WindowsAPI>, UINT msg, WPARAM wparam, LPARAM lparam) {
    LogScope scope;
    messages_[msg] = { msg, wparam, lparam };
    auto it = eventHandlers_.find(msg);
    if (it != eventHandlers_.end()) {
        return it->second->OnEvent(msg, wparam, lparam);
    }
    return std::nullopt;
}

void Window::SetSizeChangeMode(SizeChangeMode sizeChangeMode) {
    LogScope scope;
    sizeChangeMode_ = sizeChangeMode;
    
    // ウィンドウスタイルの更新（必要に応じて）
    if (descriptor_.hwnd) {
        LONG style = GetWindowLong(descriptor_.hwnd, GWL_STYLE);
        
        switch (sizeChangeMode) {
        case SizeChangeMode::None:
            // サイズ変更不可
            style &= ~(WS_SIZEBOX | WS_MAXIMIZEBOX);
            break;
        case SizeChangeMode::Normal:
            // 自由変更
            style |= (WS_SIZEBOX | WS_MAXIMIZEBOX);
            break;
        case SizeChangeMode::FixedAspect:
            // アスペクト比固定（サイズ変更は可能）
            style |= (WS_SIZEBOX | WS_MAXIMIZEBOX);
            break;
        }
        
        SetWindowLong(descriptor_.hwnd, GWL_STYLE, style);
        SetWindowPos(descriptor_.hwnd, nullptr, 0, 0, 0, 0, 
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
}

void Window::SetWindowMode(WindowMode windowMode) {
    LogScope scope;
    if (windowMode_ == windowMode || !descriptor_.hwnd) {
        return;
    }
    
    windowMode_ = windowMode;
    
    switch (windowMode) {
    case WindowMode::Window:
        // ウィンドウモード
        SetWindowLong(descriptor_.hwnd, GWL_STYLE, descriptor_.windowStyle);
        ShowWindow(descriptor_.hwnd, SW_NORMAL);
        AdjustWindowSize();
        break;
        
    case WindowMode::FullScreen:
        // フルスクリーンモード
        SetWindowLong(descriptor_.hwnd, GWL_STYLE, WS_POPUP);
        
        // モニターサイズを取得
        MONITORINFO monitorInfo = {};
        monitorInfo.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(MonitorFromWindow(descriptor_.hwnd, MONITOR_DEFAULTTOPRIMARY), &monitorInfo);
        
        SetWindowPos(descriptor_.hwnd, HWND_TOP,
                     monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
                     monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                     monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                     SWP_FRAMECHANGED);
        break;
    }
}

void Window::SetWindowTitle(const std::wstring &title) {
    LogScope scope;
    titleW_ = title;
    descriptor_.title = ConvertString(title);
    
    if (descriptor_.hwnd) {
        SetWindowText(descriptor_.hwnd, titleW_.c_str());
    }
}

void Window::SetWindowSize(int32_t width, int32_t height) {
    LogScope scope;
    size_.clientWidth = width;
    size_.clientHeight = height;
    
    // アスペクト比の再計算
    CalculateAspectRatio();
    
    // ウィンドウサイズの調整
    AdjustWindowSize();
}

void Window::SetWindowPosition(int32_t x, int32_t y) {
    LogScope scope;
    if (descriptor_.hwnd) {
        SetWindowPos(descriptor_.hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
}

void Window::RegisterWindowEvent(const std::unique_ptr<IWindowEvent> &eventHandler) {
    LogScope scope;
    UINT msg = eventHandler->kTargetMessage_;
    eventHandlers_[msg] = eventHandler->Clone();
    eventHandlers_[msg]->SetWindow(this);
}

void Window::UnregisterWindowEvent(UINT msg) {
    LogScope scope;
    eventHandlers_[msg] = nullptr;
}

const WindowMessage &Window::GetWindowMessage(UINT msg) const {
    // const メンバはスコープログ生成しない
    static const WindowMessage kEmptyMessage{ WM_NULL, 0, 0 };
    auto it = messages_.find(msg);
    if (it != messages_.end()) {
        return it->second;
    }
    return kEmptyMessage;
}

bool Window::InitializeWindow(
    WNDPROC windowProc,
    const std::wstring &title,
    int32_t width,
    int32_t height,
    DWORD windowStyle,
    const std::wstring &iconPath) {
    LogScope scope;
    // パラメータの保存
    titleW_ = title;
    descriptor_.title = ConvertString(title);
    size_.clientWidth = width;
    size_.clientHeight = height;
    descriptor_.windowStyle = windowStyle;
    descriptor_.hInstance = GetModuleHandle(nullptr);

    // アスペクト比の計算
    CalculateAspectRatio();

    // ウィンドウクラスの設定
    descriptor_.wc = {};
    descriptor_.wc.lpfnWndProc = windowProc;                   // ウィンドウプロシージャ
    descriptor_.wc.lpszClassName = titleW_.c_str();            // ウィンドウクラス名
    descriptor_.wc.hInstance = descriptor_.hInstance;          // インスタンスハンドル
    descriptor_.wc.hCursor = LoadCursor(nullptr, IDC_ARROW);   // カーソル
    descriptor_.wc.hbrBackground = nullptr;                    // 背景ブラシ（DirectXで描画するためnull）
    descriptor_.wc.lpszMenuName = nullptr;                     // メニューなし
    descriptor_.wc.cbClsExtra = 0;                             // 追加のクラスメモリなし
    descriptor_.wc.cbWndExtra = 0;                             // 追加のウィンドウメモリなし
    descriptor_.wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION); // デフォルトアイコン

    // アイコンのカスタマイズが必要ならここで読み込む
    if (!iconPath.empty()) {
        // アイコンの読み込み（例としてLoadImageを使用）
        HICON hIcon = static_cast<HICON>(LoadImage(
            descriptor_.hInstance,
            iconPath.c_str(),
            IMAGE_ICON,
            32, 32,
            LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED));
        if (hIcon) {
            descriptor_.wc.hIcon = hIcon;
        }
    }

    // ウィンドウクラスの登録
    if (!RegisterClass(&descriptor_.wc)) {
        // 既に登録済みの場合は無視
        DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    // クライアント領域のサイズからウィンドウサイズを計算
    size_.clientRect = { 0, 0, size_.clientWidth, size_.clientHeight };
    size_.windowRect = size_.clientRect;

    // ウィンドウスタイルに応じてサイズを調整
    AdjustWindowRect(&size_.windowRect, descriptor_.windowStyle, FALSE);

    // ウィンドウの作成
    descriptor_.hwnd = CreateWindowEx(
        0,                                              // 拡張ウィンドウスタイル
        descriptor_.wc.lpszClassName,                   // ウィンドウクラス名
        titleW_.c_str(),                                // ウィンドウタイトル
        descriptor_.windowStyle,                        // ウィンドウスタイル
        CW_USEDEFAULT,                                  // X座標
        CW_USEDEFAULT,                                  // Y座標
        size_.windowRect.right - size_.windowRect.left, // ウィンドウ幅
        size_.windowRect.bottom - size_.windowRect.top, // ウィンドウ高さ
        nullptr,                                        // 親ウィンドウ
        nullptr,                                        // メニューハンドル
        descriptor_.hInstance,                          // インスタンスハンドル
        this                                            // 作成パラメータ（thisポインタを渡す）
    );

    if (!descriptor_.hwnd) {
        return false;
    }

    // ウィンドウハンドルにthisポインタを関連付け
    SetWindowLongPtr(descriptor_.hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // ウィンドウを表示
    ShowWindow(descriptor_.hwnd, SW_SHOW);
    UpdateWindow(descriptor_.hwnd);

    return true;
}

void Window::ProcessMessage() {
    LogScope scope;
    MSG msg{};

    // メッセージをウィンドウプロシージャに送信
    while (PeekMessage(&msg, descriptor_.hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void Window::Cleanup() {
    LogScope scope;
    if (descriptor_.hwnd) {
        DestroyWindow(descriptor_.hwnd);
    }
    
    if (descriptor_.hInstance) {
        UnregisterClass(descriptor_.wc.lpszClassName, descriptor_.hInstance);
    }
}

void Window::CalculateAspectRatio() {
    LogScope scope;
    if (size_.clientHeight > 0) {
        size_.aspectRatio = static_cast<float>(size_.clientWidth) / static_cast<float>(size_.clientHeight);
    } else {
        size_.aspectRatio = 1.0f;
    }
}

void Window::AdjustWindowSize() {
    LogScope scope;
    if (!descriptor_.hwnd) {
        return;
    }
    
    // クライアント領域のサイズからウィンドウサイズを計算
    RECT rect = { 0, 0, size_.clientWidth, size_.clientHeight };
    AdjustWindowRect(&rect, descriptor_.windowStyle, FALSE);
    
    // ウィンドウサイズを更新
    SetWindowPos(descriptor_.hwnd, nullptr, 0, 0, 
                 rect.right - rect.left, rect.bottom - rect.top,
                 SWP_NOMOVE | SWP_NOZORDER);
}

} // namespace KashipanEngine