#include "Window.h"
#include "Core/WindowsAPI.h"
#include "Core/DirectXCommon.h"
#include "Utilities/Conversion/ConvertString.h"
#include <cassert>
#include <algorithm>

#include "Core/WindowsAPI/WindowEvents/DefaultEvents.h"
#include "Graphics/Renderer.h"

namespace KashipanEngine {

namespace {
/// @brief ウィンドウ管理用マップ
std::unordered_map<HWND, std::unique_ptr<Window>> sWindowMap;
/// @brief 破棄保留中ウィンドウリスト
std::vector<HWND> sPendingDestroy; 
}

Window::Window(Passkey<Window>, WindowType windowType, const std::wstring &title, int32_t width, int32_t height, DWORD windowStyle, const std::wstring &iconPath) {
    LogScope scope;
    bool result = InitializeWindow(sWindowsAPI->WindowProc, windowType, title, width, height, windowStyle, iconPath);
    if (!result) assert("Window initialization failed");
    messages_.reserve(kMaxMessages);
    eventHandlers_.reserve(kMaxMessages);

    using namespace WindowDefaultEvent;
    RegisterWindowEvent<DestroyEvent>();
    RegisterWindowEvent<CloseEvent>();
    RegisterWindowEvent<SizeEvent>();
    RegisterWindowEvent<EnterSizeMoveEvent>();
    RegisterWindowEvent<ExitSizeMoveEvent>();
    RegisterWindowEvent<ActivateEvent>();
    RegisterWindowEvent<GetMinMaxInfoEvent>();
    RegisterWindowEvent<SizingEvent>();
    RegisterWindowEvent<SysCommandCloseEvent>();
}

Window::~Window() {
    LogScope scope;
    // 実際の破棄は CommitDestroy() 側に委ねるためここでは何もしない
}

void Window::SetDefaultParams(Passkey<GameEngine>, const std::string &title, int32_t width, int32_t height, DWORD style, const std::string &iconPath) {
    LogScope scope;
    windowDefaultTitle = title;
    windowDefaultWidth = width;
    windowDefaultHeight = height;
    windowDefaultStyle = style;
    windowDefaultIconPath = iconPath;
}

void Window::AllDestroy(Passkey<GameEngine> passkey) {
    LogScope scope;
    // まず全ウィンドウに破棄通知（二重通知防止）
    for (auto &pair : sWindowMap) {
        if (!pair.second->IsPendingDestroy()) {
            pair.second->DestroyNotify();
        }
    }
    CommitDestroy(passkey); // 受け取った Passkey を利用
}

Window *Window::GetWindow(HWND hwnd) {
    auto it = sWindowMap.find(hwnd);
    if (it != sWindowMap.end()) return it->second.get();
    return nullptr;
}

std::vector<Window *> Window::GetWindows(const std::string &title) {
    std::vector<Window *> windows;
    for (auto &pair : sWindowMap) if (pair.second->descriptor_.title == title) windows.push_back(pair.second.get());
    return windows;
}

#if defined(USE_IMGUI)
HWND Window::GetFirstWindowHwndForImGui(Passkey<ImGuiManager>) {
    for (auto &pair : sWindowMap) {
        if (pair.second->GetWindowType() == WindowType::Overlay) continue;
        return pair.first;
    }
    return nullptr;
}
#endif

Window *Window::GetWindow(const std::string &title) {
    for (auto &pair : sWindowMap) if (pair.second->descriptor_.title == title) return pair.second.get();
    return nullptr;
}

size_t Window::GetWindowCount() { return sWindowMap.size(); }

bool Window::IsExist(HWND hwnd) { return sWindowMap.find(hwnd) != sWindowMap.end(); }
bool Window::IsExist(Window *window) { for (auto &pair : sWindowMap) if (pair.second.get() == window) return true; return false; }
bool Window::IsExist(const std::string &title) { for (auto &pair : sWindowMap) if (pair.second->descriptor_.title == title) return true; return false; }

void Window::Update(Passkey<GameEngine>) {
    LogScope scope;
    for (auto &pair : sWindowMap) {
        Window *window = pair.second.get();
        window->ClearMessages();
        window->ProcessMessage();
        // WM_DESTROY メッセージを受信 or Destroy() 呼び出しで pending フラグセット
        if (window->IsDestroyed() || window->IsPendingDestroy()) {
            if (!window->IsPendingDestroy()) {
                // メッセージ経由で初めて検出した場合通知化
                window->DestroyNotify();
            }
        }
    }
    // Update 内では破棄実行しない
}

void Window::CommitDestroy(Passkey<GameEngine>) {
    LogScope scope;
    // 一度クリアして、最新の pending 状態から再構築
    sPendingDestroy.clear();

    // pending リストに無いウィンドウで pendingDestroy_ フラグが立っているものも拾う
    struct PendingInfo { HWND hwnd; int depth; };
    std::vector<PendingInfo> pendingInfos;
    pendingInfos.reserve(sWindowMap.size());

    auto calcDepth = [](Window* w) {
        int d = 0;
        for (auto* p = w->parentWindow_; p != nullptr; p = p->parentWindow_) ++d;
        return d;
    };

    for (auto &pair : sWindowMap) {
        if (pair.second->IsPendingDestroy()) {
            HWND hwnd = pair.second->GetWindowHandle();
            if (hwnd) pendingInfos.push_back({ hwnd, calcDepth(pair.second.get()) });
        }
    }

    // 子(深い階層)から破棄するため、depth の大きい順に並べる
    std::sort(pendingInfos.begin(), pendingInfos.end(), [](const PendingInfo& a, const PendingInfo& b){ return a.depth > b.depth; });

    for (const auto& info : pendingInfos) sPendingDestroy.push_back(info.hwnd);

    // 実際の破棄処理（子→親の順)
    for (HWND hwnd : sPendingDestroy) {
        auto it = sWindowMap.find(hwnd);
        if (it == sWindowMap.end()) continue;
        Window *window = it->second.get();
        Log(Translation("engine.window.destroy.start") + window->descriptor_.title, LogSeverity::Debug);
        // 親子リンク解除（物理 SetParent は OS に任せる）
        window->DetachAllChildrenUnsafe(false);
        window->DetachFromParentUnsafe(false);
        // Win32 ウィンドウ破棄 & クラス解除
        window->Cleanup();
        Log(Translation("engine.window.destroy.end") + window->descriptor_.title, LogSeverity::Debug);
        sWindowMap.erase(it);
    }
    sPendingDestroy.clear();

    if (sWindowMap.empty()) PostQuitMessage(0);
}

void Window::Draw(Passkey<GameEngine>) {
    LogScope scope;

    // 描画順制御: オーバーレイを最初に、その後 親->子 の階層（浅い順）に描画。
    struct DrawInfo { Window* w; int depth; };
    std::vector<DrawInfo> drawInfos;
    drawInfos.reserve(sWindowMap.size());

    auto calcDepth = [](Window* w){ int d = 0; for (auto* p = w->parentWindow_; p != nullptr; p = p->parentWindow_) ++d; return d; };

    for (auto &pair : sWindowMap) {
        Window *w = pair.second.get();
        if (w->IsPendingDestroy()) continue; // 破棄予定はスキップ
        if (!w->IsVisible()) continue;       // 非表示はスキップ
        drawInfos.push_back({ w, calcDepth(w) });
    }

    std::stable_sort(drawInfos.begin(), drawInfos.end(), [](const DrawInfo &a, const DrawInfo &b){
        // オーバーレイ優先（先頭）
        if (a.w->GetWindowType() != b.w->GetWindowType()) {
            if (a.w->GetWindowType() == WindowType::Overlay) return true;
            if (b.w->GetWindowType() == WindowType::Overlay) return false;
        }
        // 深さ昇順（親→子）
        if (a.depth != b.depth) return a.depth < b.depth;
        // HWND で安定化（生成順不定の unordered_map 対策）
        return reinterpret_cast<uintptr_t>(a.w->GetWindowHandle()) < reinterpret_cast<uintptr_t>(b.w->GetWindowHandle());
    });

    for (auto &info : drawInfos) {
        Window *window = info.w;

        // 子ウィンドウの残像対策: オーバーレイは常に再描画要求（全子含む）
        if (window->GetWindowType() == WindowType::Overlay && window->GetWindowHandle()) {
            RedrawWindow(window->GetWindowHandle(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
        }

        if (window->dx12SwapChain_) window->dx12SwapChain_->BeginDraw(Passkey<Window>{});
    }
}

Window *Window::CreateNormal(const std::string &title, int32_t width, int32_t height, DWORD style, const std::string &iconPath) {
    LogScope scope;
    Log(Translation("engine.window.create.start") + (title.empty() ? windowDefaultTitle : title), LogSeverity::Debug);

    std::wstring windowTitle = title.empty() ? ConvertString(windowDefaultTitle) : ConvertString(title);
    int32_t windowWidth = (width <= 0) ? windowDefaultWidth : width;
    int32_t windowHeight = (height <= 0) ? windowDefaultHeight : height;
    DWORD windowStyle = (style == 0) ? WS_OVERLAPPEDWINDOW : style;
    std::wstring windowIconPath = iconPath.empty() ? ConvertString(windowDefaultIconPath) : ConvertString(iconPath);

    auto window = std::make_unique<Window>(Passkey<Window>{}, WindowType::Normal, windowTitle, windowWidth, windowHeight, windowStyle, windowIconPath);
    HWND hwnd = window->GetWindowHandle();

    sWindowMap[hwnd] = std::move(window);
    sWindowsAPI->RegisterWindow({}, sWindowMap[hwnd].get());
    sWindowMap[hwnd]->dx12SwapChain_ = sDirectXCommon->CreateSwapChain({}, SwapChainType::ForHwnd, hwnd, windowWidth, windowHeight);
    auto cmdList = sWindowMap[hwnd]->dx12SwapChain_->GetRecordedCommandList(Passkey<Window>{});
    sRenderer->RegisterWindow(Passkey<Window>{}, hwnd, cmdList);

    Log(Translation("engine.window.create.end") + (title.empty() ? windowDefaultTitle : title), LogSeverity::Debug);
    return sWindowMap[hwnd].get();
}

Window *Window::CreateOverlay(const std::string &title, int32_t width, int32_t height, bool clickThrough, const std::string &iconPath) {
    LogScope scope;
    Log(Translation("engine.window.create.overlay.start") + (title.empty() ? windowDefaultTitle : title), LogSeverity::Debug);

    std::wstring windowTitle = title.empty() ? ConvertString(windowDefaultTitle) : ConvertString(title);
    int32_t windowWidth = (width <= 0) ? windowDefaultWidth : width;
    int32_t windowHeight = (height <= 0) ? windowDefaultHeight : height;
    std::wstring windowIconPath = iconPath.empty() ? ConvertString(windowDefaultIconPath) : ConvertString(iconPath);

    DWORD style = WS_POPUP;
    DWORD exStyle = WS_EX_NOREDIRECTIONBITMAP;
    exStyle |= WS_EX_CONTEXTHELP;
    // Debug 中は TOPMOST を避ける / Release では付与
#if defined(DEBUG_BUILD) || defined(DEVELOPMENT_BUILD)
    bool isDebugging = ::IsDebuggerPresent() != 0;
    if (!isDebugging) exStyle |= WS_EX_TOPMOST;
#else
    exStyle |= WS_EX_TOPMOST;
#endif
    if (clickThrough) exStyle |= WS_EX_TRANSPARENT | WS_EX_LAYERED;

    auto window = std::make_unique<Window>(Passkey<Window>{}, WindowType::Overlay, windowTitle, windowWidth, windowHeight, style, windowIconPath);
    HWND hwnd = window->GetWindowHandle();

    ::SetWindowLong(hwnd, GWL_EXSTYLE, ::GetWindowLong(hwnd, GWL_EXSTYLE) | exStyle);
    ::SetWindowPos(hwnd,
#if defined(DEBUG_BUILD) || defined(DEVELOPMENT_BUILD)
        (exStyle & WS_EX_TOPMOST) ? HWND_TOPMOST : HWND_NOTOPMOST,
#else
        HWND_TOPMOST,
#endif
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#if defined(DEBUG_BUILD) || defined(DEVELOPMENT_BUILD)
    ::RegisterHotKey(hwnd, 0xDEAD, MOD_CONTROL | MOD_ALT, VK_F12);
#endif

    sWindowMap[hwnd] = std::move(window);
    sWindowsAPI->RegisterWindow({}, sWindowMap[hwnd].get());
    sWindowMap[hwnd]->dx12SwapChain_ = sDirectXCommon->CreateSwapChain({}, SwapChainType::ForComposition, hwnd, windowWidth, windowHeight);
    sWindowMap[hwnd]->RegisterWindowEvent<WindowDefaultEvent::ClickThroughEvent>(clickThrough);
    auto cmdList = sWindowMap[hwnd]->dx12SwapChain_->GetRecordedCommandList(Passkey<Window>{});
    sRenderer->RegisterWindow(Passkey<Window>{}, hwnd, cmdList);

    Log(Translation("engine.window.create.overlay.end") + (title.empty() ? windowDefaultTitle : title), LogSeverity::Debug);
    return sWindowMap[hwnd].get();
}

void Window::DestroyNotify() {
    LogScope scope;
    if (!isPendingDestroy_) {
        // 子ウィンドウにも破棄通知を伝播させる（再帰）
        // 子リストが変化しても安全なようにコピーしてから走査
        auto children = childWindows_;
        for (auto *child : children) {
            if (child && !child->isPendingDestroy_) {
                child->DestroyNotify();
            }
        }

        isPendingDestroy_ = true;
        if (descriptor_.hwnd) {
            // Post WM_CLOSE で標準破棄フロー誘導 + 登録解除 + SwapChain 破棄シグナル
            ::PostMessage(descriptor_.hwnd, WM_CLOSE, 0, 0);
            sWindowsAPI->UnregisterWindow({}, descriptor_.hwnd);
            sDirectXCommon->DestroySwapChainSignal({}, descriptor_.hwnd);
        }
    }
}

std::optional<LRESULT> Window::HandleEvent(Passkey<WindowsAPI>, UINT msg, WPARAM wparam, LPARAM lparam) {
    LogScope scope;
    messages_[msg] = { msg, wparam, lparam };
    auto it = eventHandlers_.find(msg);
    if (it == eventHandlers_.end()) return std::nullopt;
    return std::visit([&](auto &stored) -> std::optional<LRESULT> {
        using T = std::decay_t<decltype(stored)>;
        if constexpr (std::is_same_v<T, std::unique_ptr<IWindowEvent>>) {
            if (stored) return stored->OnEvent(msg, wparam, lparam);
            return std::nullopt;
        } else {
            return stored.OnEvent(msg, wparam, lparam);
        }
    }, it->second);
}

void Window::SetSizeChangeMode(SizeChangeMode sizeChangeMode) {
    LogScope scope;
    sizeChangeMode_ = sizeChangeMode;
    if (descriptor_.hwnd) {
        LONG style = GetWindowLong(descriptor_.hwnd, GWL_STYLE);
        switch (sizeChangeMode) {
            case SizeChangeMode::None: style &= ~(WS_SIZEBOX | WS_MAXIMIZEBOX); break;
            case SizeChangeMode::Normal: style |= (WS_SIZEBOX | WS_MAXIMIZEBOX); break;
            case SizeChangeMode::FixedAspect: style |= (WS_SIZEBOX | WS_MAXIMIZEBOX); break;
        }
        SetWindowLong(descriptor_.hwnd, GWL_STYLE, style);
        SetWindowPos(descriptor_.hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
}

void Window::SetWindowMode(WindowMode windowMode) {
    LogScope scope;
    if (windowMode_ == windowMode || !descriptor_.hwnd) return;
    windowMode_ = windowMode;
    switch (windowMode) {
        case WindowMode::Window:
            SetWindowLong(descriptor_.hwnd, GWL_STYLE, descriptor_.windowStyle);
            ShowWindow(descriptor_.hwnd, SW_NORMAL);
            AdjustWindowSize();
            break;
        case WindowMode::FullScreen: {
            SetWindowLong(descriptor_.hwnd, GWL_STYLE, WS_POPUP);
            MONITORINFO monitorInfo{}; monitorInfo.cbSize = sizeof(MONITORINFO);
            GetMonitorInfo(MonitorFromWindow(descriptor_.hwnd, MONITOR_DEFAULTTOPRIMARY), &monitorInfo);
            SetWindowPos(descriptor_.hwnd, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
                         monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                         monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                         SWP_FRAMECHANGED);
        } break;
    }
}

void Window::SetWindowTitle(const std::wstring &title) {
    LogScope scope;
    titleW_ = title;
    descriptor_.title = ConvertString(title);
    if (descriptor_.hwnd) SetWindowText(descriptor_.hwnd, titleW_.c_str());
}

void Window::SetWindowTitle(const std::string &title) {
    LogScope scope;
    titleW_ = ConvertString(title);
    descriptor_.title = title;
    if (descriptor_.hwnd) SetWindowText(descriptor_.hwnd, titleW_.c_str());
}

void Window::SetWindowSize(int32_t width, int32_t height) {
    LogScope scope;
    size_.clientWidth = width;
    size_.clientHeight = height;
    CalculateAspectRatio();
    AdjustWindowSize();
}

void Window::SetWindowPosition(int32_t x, int32_t y) {
    LogScope scope;
    if (descriptor_.hwnd) SetWindowPos(descriptor_.hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void Window::SetWindowVisible(bool visible) {
    LogScope scope;
    if (descriptor_.hwnd) ShowWindow(descriptor_.hwnd, visible ? SW_SHOW : SW_HIDE);
}

void Window::DetachFromParentUnsafe(bool applyNative) {
    if (!parentWindow_) return;
    parentWindow_->RemoveChildPointerUnsafe(this);
    if (applyNative && descriptor_.hwnd) SetParent(descriptor_.hwnd, nullptr);
    parentWindow_ = nullptr;
}

void Window::DetachAllChildrenUnsafe(bool applyNative) {
    for (auto *child : childWindows_) {
        if (!child) continue;
        child->parentWindow_ = nullptr;
        if (applyNative && child->descriptor_.hwnd) SetParent(child->descriptor_.hwnd, nullptr);
    }
    childWindows_.clear();
}

void Window::RemoveChildPointerUnsafe(Window *child) {
    if (!child) return;
    auto it = std::find(childWindows_.begin(), childWindows_.end(), child);
    if (it != childWindows_.end()) childWindows_.erase(it);
}

void Window::SetWindowParent(HWND parentHwnd, bool applyNative) {
    LogScope scope;
    if (!descriptor_.hwnd) return;
    if (parentHwnd == nullptr) { ClearWindowParent(applyNative); return; }
    if (parentWindow_ && parentWindow_->GetWindowHandle() == parentHwnd) return;
    DetachFromParentUnsafe(applyNative);
    if (applyNative) SetParent(descriptor_.hwnd, parentHwnd);
    if (Window *pw = GetWindow(parentHwnd)) { parentWindow_ = pw; pw->childWindows_.push_back(this); }
    else { parentWindow_ = nullptr; }
}

void Window::SetWindowParent(Window *parentWindow, bool applyNative) {
    LogScope scope;
    if (!descriptor_.hwnd) return;
    if (!parentWindow) { ClearWindowParent(applyNative); return; }
    if (parentWindow_ == parentWindow) return;
    DetachFromParentUnsafe(applyNative);
    if (applyNative) SetParent(descriptor_.hwnd, parentWindow->GetWindowHandle());
    parentWindow_ = parentWindow;
    parentWindow_->childWindows_.push_back(this);
}

void Window::SetWindowChild(HWND childHwnd, bool applyNative) {
    LogScope scope;
    if (!descriptor_.hwnd || !childHwnd) return;
    if (Window *child = GetWindow(childHwnd)) { SetWindowChild(child, applyNative); }
    else if (applyNative) { SetParent(childHwnd, descriptor_.hwnd); }
}

void Window::SetWindowChild(Window *childWindow, bool applyNative) {
    LogScope scope;
    if (!descriptor_.hwnd || !childWindow || childWindow == this) return;
    childWindow->SetWindowParent(this, applyNative);
}

void Window::ClearWindowParent(bool applyNative) { LogScope scope; DetachFromParentUnsafe(applyNative); }
void Window::ClearWindowChild(bool applyNative) { LogScope scope; DetachAllChildrenUnsafe(applyNative); }

void Window::UnregisterWindowEvent(UINT msg) { LogScope scope; eventHandlers_.erase(msg); }

const WindowMessage &Window::GetWindowMessage(UINT msg) const {
    static const WindowMessage kEmptyMessage{ WM_NULL, 0, 0 };
    auto it = messages_.find(msg);
    if (it != messages_.end()) return it->second;
    return kEmptyMessage;
}

bool Window::InitializeWindow(WNDPROC windowProc, WindowType windowType, const std::wstring &title, int32_t width, int32_t height, DWORD windowStyle, const std::wstring &iconPath) {
    LogScope scope;
    // パラメータの保存
    windowType_ = windowType;
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
        if (error != ERROR_CLASS_ALREADY_EXISTS) return false;
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
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void Window::Cleanup() {
    LogScope scope;
    DetachAllChildrenUnsafe(false);
    DetachFromParentUnsafe(false);
    if (descriptor_.hwnd) DestroyWindow(descriptor_.hwnd);
    if (descriptor_.hInstance) UnregisterClass(descriptor_.wc.lpszClassName, descriptor_.hInstance);
    descriptor_.hwnd = nullptr;
}

void Window::CalculateAspectRatio() {
    LogScope scope;
    size_.aspectRatio = (size_.clientHeight > 0) ? static_cast<float>(size_.clientWidth) / static_cast<float>(size_.clientHeight) : 1.0f;
}

void Window::AdjustWindowSize() {
    LogScope scope;
    if (!descriptor_.hwnd) return;
    RECT rect = { 0, 0, size_.clientWidth, size_.clientHeight };
    AdjustWindowRect(&rect, descriptor_.windowStyle, FALSE);
    SetWindowPos(descriptor_.hwnd, nullptr, 0, 0,
        rect.right - rect.left, rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER);
    if (dx12SwapChain_) {
        if (size_.clientWidth <= 0) size_.clientWidth = 1;
        if (size_.clientHeight <= 0) size_.clientHeight = 1;
        dx12SwapChain_->ResizeSignal(Passkey<Window>{}, size_.clientWidth, size_.clientHeight);
    }
}

} // namespace KashipanEngine