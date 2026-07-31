// KashipanEngine のプロジェクトランチャー
//
// エディター本体（KashipanEngine.exe）とは別のexeとして、プロジェクトの一覧表示・
// 新規作成・起動だけを行う。選んだプロジェクトは
// KashipanEngine.exe --project <名前> という形で引き渡す。
//
// 画面はWebView2でHTML/CSSを描画し、エンジンリファレンスと同じ見た目にしている。
// WebView2ランタイムが無い環境では、Win32コントロールで組んだ簡易UIへ自動的に切り替わる。

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <memory>

#include "Core/ProjectPaths.h"
#include "LauncherFallbackUI.h"
#include "LauncherUI.h"
#include "LauncherWebViewUI.h"

using namespace KashipanEngine;

namespace {

/// @brief WebView2版の画面に適したクライアント領域のサイズ（96 DPI 基準）
constexpr int kRichClientWidth = 760;
constexpr int kRichClientHeight = 600;

constexpr DWORD kWindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME;

/// @brief 現在使用している画面
std::unique_ptr<Launcher::ILauncherUI> gUI;
/// @brief 96 DPI に対する表示倍率
float gDpiScale = 1.0f;

int Scaled(int value) {
    return static_cast<int>(static_cast<float>(value) * gDpiScale + 0.5f);
}

/// @brief クライアント領域が指定サイズになるようウィンドウ全体の大きさを合わせる
void ResizeClientArea(HWND window, int clientWidth, int clientHeight) {
    RECT rect{ 0, 0, Scaled(clientWidth), Scaled(clientHeight) };
    AdjustWindowRectExForDpi(&rect, kWindowStyle, FALSE, 0, GetDpiForWindow(window));
    SetWindowPos(window, nullptr, 0, 0,
        rect.right - rect.left, rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/// @brief Win32コントロール版の画面へ切り替える
/// @details WebView2の初期化に失敗した場合に呼ばれる。この画面は固定レイアウトのため、
///          ウィンドウの大きさも合わせて縮める。
void SwitchToFallbackUI(HWND window) {
    gUI = std::make_unique<Launcher::FallbackUI>(gDpiScale);
    if (!gUI->Create(window)) {
        DestroyWindow(window);
        return;
    }
    SetWindowLongPtrW(window, GWL_STYLE, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);
    ResizeClientArea(window, Launcher::FallbackUI::kClientWidth, Launcher::FallbackUI::kClientHeight);
}

LRESULT CALLBACK LauncherWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_COMMAND:
        if (gUI && gUI->HandleCommand(wparam)) return 0;
        break;

    case WM_SIZE:
        if (gUI) gUI->Resize(LOWORD(lparam), HIWORD(lparam));
        return 0;

    case Launcher::kWebViewFailedMessage:
        // WebView2の初期化はウィンドウ生成後に非同期で失敗することがあるため、
        // その通知を受けてからでもWin32版へ切り替えられるようにしている
        SwitchToFallbackUI(window);
        return 0;

    case WM_CTLCOLORSTATIC:
        // STATICコントロールの背景をウィンドウ背景に合わせる（灰色の帯が出るのを防ぐ）
        SetBkMode(reinterpret_cast<HDC>(wparam), TRANSPARENT);
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));

    case WM_DESTROY:
        gUI.reset();
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand) {
    // WebView2はCOMを使うため、UIスレッドをSTAとして初期化しておく
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return -1;

    // 高DPI環境でもぼやけないよう、システムDPIに合わせて自前でスケーリングする
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

    // Projects/ と AssetsTemplate/ の位置を知るためにエンジンルートだけ解決する
    // （プロジェクトを開くのはエディター側の仕事なので、ここでは開かない）
    ProjectPaths::InitializeEngineRootOnly({});

    gDpiScale = static_cast<float>(GetDpiForSystem()) / 96.0f;

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = LauncherWindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    windowClass.lpszClassName = L"KashipanEngineLauncher";
    if (!RegisterClassExW(&windowClass)) return -1;

    // どちらの画面を使うかでウィンドウの大きさが違うため、生成前に判定しておく
    const bool useWebView = Launcher::WebViewUI::IsAvailable();
    const int clientWidth = useWebView ? kRichClientWidth : Launcher::FallbackUI::kClientWidth;
    const int clientHeight = useWebView ? kRichClientHeight : Launcher::FallbackUI::kClientHeight;

    RECT windowRect{ 0, 0, Scaled(clientWidth), Scaled(clientHeight) };
    const DWORD style = useWebView ? kWindowStyle : (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);
    AdjustWindowRectExForDpi(&windowRect, style, FALSE, 0, GetDpiForSystem());

    HWND window = CreateWindowExW(
        0, windowClass.lpszClassName, L"KashipanEngine プロジェクトランチャー",
        style, CW_USEDEFAULT, CW_USEDEFAULT,
        windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
        nullptr, nullptr, instance, nullptr);
    if (!window) return -1;

    if (useWebView) {
        auto webViewUI = std::make_unique<Launcher::WebViewUI>();
        if (webViewUI->Create(window)) {
            gUI = std::move(webViewUI);
        } else {
            SwitchToFallbackUI(window);
        }
    } else {
        SwitchToFallbackUI(window);
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        // Enterキーやタブ移動などのダイアログ的な操作を効かせる（Win32版で必要）
        if (IsDialogMessageW(window, &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    gUI.reset();
    CoUninitialize();
    return static_cast<int>(message.wParam);
}
