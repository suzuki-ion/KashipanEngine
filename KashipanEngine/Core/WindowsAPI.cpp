#include "WindowsAPI.h"
#include <cassert>
#include <memory>
#include <unordered_map>
#include <optional>
#include <shellapi.h>
#include <ShellScalingAPI.h>
#include "Core/Window.h"
#include "Utilities/Conversion/ConvertString.h"

#pragma comment(lib, "Shcore.lib")

namespace KashipanEngine {

namespace {
// ウィンドウインスタンスのマップ
std::unordered_map<HWND, Window*> sWindowMap;

// タスクバーの辺を判定
static UINT DetermineTaskbarEdge(const RECT &tb, const RECT &mon) {
    int monW = mon.right - mon.left;
    int monH = mon.bottom - mon.top;
    int tbW = tb.right - tb.left;
    int tbH = tb.bottom - tb.top;
    if (tbW >= monW && tbH < monH) {
        return (tb.top <= mon.top) ? ABE_TOP : ABE_BOTTOM;
    }
    if (tbH >= monH && tbW < monW) {
        return (tb.left <= mon.left) ? ABE_LEFT : ABE_RIGHT;
    }
    // 距離で近い辺
    int distTop = abs(tb.top - mon.top);
    int distBottom = abs(mon.bottom - tb.bottom);
    int distLeft = abs(tb.left - mon.left);
    int distRight = abs(mon.right - tb.right);
    int m = std::min(std::min(distTop, distBottom), std::min(distLeft, distRight));
    if (m == distTop) return ABE_TOP;
    if (m == distBottom) return ABE_BOTTOM;
    if (m == distLeft) return ABE_LEFT;
    return ABE_RIGHT;
}

// DPI取得（失敗時は 96 を返す）
static void FetchMonitorDpi(HMONITOR mon, UINT &dpiX, UINT &dpiY) {
    dpiX = dpiY = 96;
    if (!mon) return;
    UINT x = 0, y = 0;
    if (SUCCEEDED(GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &x, &y))) {
        dpiX = x; dpiY = y; return; }
    // フォールバック: デバイスコンテキストから推定
    HDC hdc = GetDC(nullptr);
    if (hdc) {
        dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
        dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(nullptr, hdc);
    }
}

// リフレッシュレート取得（Hz）
static float FetchMonitorRefreshRate(const MONITORINFOEXW &mi) {
    DEVMODEW dm{}; dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsExW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm, 0)) {
        if (dm.dmDisplayFrequency > 1) return static_cast<float>(dm.dmDisplayFrequency);
    }
    return 0.0f;
}

// タスクバー候補を列挙
struct TaskbarWindow { HWND hwnd; RECT rect; HMONITOR monitor; UINT edge; };
static std::vector<TaskbarWindow> EnumerateTaskbars() {
    std::vector<TaskbarWindow> result;
    // プライマリ
    if (HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        RECT rc{}; GetWindowRect(primary, &rc);
        HMONITOR mon = MonitorFromWindow(primary, MONITOR_DEFAULTTOPRIMARY);
        UINT edge = (UINT)-1;
        APPBARDATA abd{}; abd.cbSize = sizeof(abd); abd.hWnd = primary;
        if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd)) {
            rc = abd.rc; edge = abd.uEdge; }
        result.push_back({ primary, rc, mon, edge });
    }
    // セカンダリ
    HWND h = nullptr;
    while ((h = FindWindowExW(nullptr, h, L"Shell_SecondaryTrayWnd", nullptr)) != nullptr) {
        RECT rc{}; if (!GetWindowRect(h, &rc)) continue;
        HMONITOR mon = MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST);
        result.push_back({ h, rc, mon, (UINT)-1 });
    }
    return result;
}
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

WindowsAPI::WindowsAPI(Passkey<GameEngine>) {
    LogScope scope;
    Log(Translation("engine.windowsapi.initialize.start"), LogSeverity::Debug);
    Log(Translation("engine.windowsapi.initialize.end"), LogSeverity::Debug);
}

WindowsAPI::~WindowsAPI() {
    LogScope scope;
    Log(Translation("engine.windowsapi.finalize.start"), LogSeverity::Debug);
    Log(Translation("engine.windowsapi.finalize.end"), LogSeverity::Debug);
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

std::optional<MonitorInfo> WindowsAPI::QueryMonitorInfo(HMONITOR monitor) {
    // 対象決定（nullptrならプライマリ）
    if (!monitor) {
        POINT pt{ 0,0 }; monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    }
    auto all = QueryAllMonitorInfos();
    for (auto &m : all) {
        if (m.MonitorHandle() == monitor) return m;
    }
    return std::nullopt;
}

std::vector<MonitorInfo> WindowsAPI::QueryAllMonitorInfos() {
    std::vector<MonitorInfo> infos;
    auto taskbars = EnumerateTaskbars();

    // 列挙コールバック
    struct EnumCtx { std::vector<MonitorInfo>* infos; const std::vector<TaskbarWindow>* taskbars; };
    EnumCtx ctx{ &infos, &taskbars };

    auto enumProc = [](HMONITOR hMon, HDC, LPRECT, LPARAM lp)->BOOL {
        auto *c = reinterpret_cast<EnumCtx*>(lp);
        MONITORINFOEXW mi{}; mi.cbSize = sizeof(mi);
        if (!GetMonitorInfoW(hMon, &mi)) return TRUE; // 継続

        MonitorInfo info;
        info.monitor_ = hMon;
        info.monitorRect_ = mi.rcMonitor;
        info.workArea_ = mi.rcWork;
        info.width_ = mi.rcMonitor.right - mi.rcMonitor.left;
        info.height_ = mi.rcMonitor.bottom - mi.rcMonitor.top;
        FetchMonitorDpi(hMon, info.dpiX_, info.dpiY_);
        info.dpiScaleX_ = static_cast<float>(info.dpiX_) / 96.0f;
        info.dpiScaleY_ = static_cast<float>(info.dpiY_) / 96.0f;
        info.fps_ = FetchMonitorRefreshRate(mi);

        // タスクバー割当
        for (auto &tb : *c->taskbars) {
            if (tb.monitor == hMon) {
                info.taskbarRect_ = tb.rect;
                info.taskbarEdge_ = (tb.edge == (UINT)-1) ? DetermineTaskbarEdge(tb.rect, mi.rcMonitor) : tb.edge;
                break;
            }
        }

        c->infos->push_back(info);
        return TRUE; // 継続
    };

    EnumDisplayMonitors(nullptr, nullptr, enumProc, reinterpret_cast<LPARAM>(&ctx));
    return infos;
}

} // namespace KashipanEngine