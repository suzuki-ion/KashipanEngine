#include "Splash/SplashScreen.h"
#include "Splash/SplashAnimation.h"
#include "Core/ProjectPaths.h"
#include "Utilities/Conversion/ConvertString.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
namespace {
BOOL CALLBACK FindOwnSplash(HWND hwnd, LPARAM parameter) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    wchar_t name[128]{};
    GetClassNameW(hwnd, name, 128);
    if (pid == GetCurrentProcessId() && std::wstring(name) == L"KashipanEngineStartupLogo") {
        *reinterpret_cast<HWND *>(parameter) = hwnd;
        return FALSE;
    }
    return TRUE;
}
void Pump() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
}
HWND OwnSplash() {
    HWND hwnd = nullptr;
    EnumWindows(FindOwnSplash, reinterpret_cast<LPARAM>(&hwnd));
    return hwnd;
}
bool ValidateWindow(HWND hwnd) {
    const auto style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    const auto exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    RECT client{};
    GetClientRect(hwnd, &client);
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
    RECT frame{};
    AdjustWindowRectExForDpi(&frame, static_cast<DWORD>(style & ~WS_VISIBLE), FALSE,
        static_cast<DWORD>(exStyle), GetDpiForWindow(hwnd));
    const auto expected = KashipanEngine::SplashAnimation::ChooseClientSize(
        monitor.rcMonitor.right - monitor.rcMonitor.left, monitor.rcMonitor.bottom - monitor.rcMonitor.top,
        monitor.rcWork.right - monitor.rcWork.left - (frame.right - frame.left),
        monitor.rcWork.bottom - monitor.rcWork.top - (frame.bottom - frame.top));
    return (style & WS_CAPTION) == WS_CAPTION && (style & WS_SYSMENU) && (style & WS_MINIMIZEBOX) &&
        !(style & (WS_THICKFRAME | WS_MAXIMIZEBOX | WS_POPUP)) && (exStyle & WS_EX_APPWINDOW) &&
        !(exStyle & (WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE)) &&
        client.right == expected.width && client.bottom == expected.height;
}
}
// Standalone stubs: native splash is linked unchanged; no engine/GPU is created.
namespace KashipanEngine {
static bool missingAssets = false;
class GameEngine { public:
    static void Ready() { SplashScreen::NotifyReady({}); }
    static void Finish() { SplashScreen::Finish({}); }
};
class Window { public:
    static HWND Claim() { return SplashScreen::ClaimWindow({}); }
    static void Detach(HWND hwnd) { SplashScreen::DetachWindow({}, hwnd); }
    static void Abandon(HWND hwnd) { SplashScreen::AbandonWindow({}, hwnd); }
};
LRESULT CALLBACK GameWindowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_APP + 50) return 42;
    return DefWindowProcW(hwnd, message, wp, lp);
}
std::string ProjectPaths::InEngineRoot(const std::string &path) { return std::filesystem::absolute(missingAssets ? "Generated/StartupSplashRegression/missing" : path).generic_string(); }
int Execute(PasskeyForWinMain, const std::string &mode) {
    std::ofstream log("Generated/StartupSplashRegression/" + mode + ".log");
    const auto start = GetTickCount64();
    missingAssets = mode == "missing" || mode == "handoff-missing";
    SplashScreen::Show({});
    const bool custom = mode.find("custom") != std::string::npos;
    if (custom) {
        const auto folder=std::filesystem::absolute("Generated/StartupSplashRegression/customproject/StartupAnimation");
        std::filesystem::create_directories(folder);
        std::ofstream package(folder/"animation.json");
        package << R"({"startupAnimationVersion":1,"sceneObjects":[
            {"objectID":"common","components":[{"type":"LoadingAnimationSection","data":{"customData":{"section":3}}},
            {"type":"SpriteRenderer","data":{"customData":{}},"startupMaterial":{"image":""}}]},
            {"objectID":"intro","components":[{"type":"LoadingAnimationSection","data":{"customData":{"section":0,"endMode":0,"seconds":-2}}}]},
            {"objectID":"outro","components":[{"type":"LoadingAnimationSection","data":{"customData":{"section":2,"endMode":0,"seconds":0.2}}}]}]})";
        package.close();
        SplashScreen::ConfigureProject({}, folder.parent_path().generic_string());
    } else SplashScreen::ConfigureProject({}, "");
    const DWORD delay = mode == "slow" ? 20000 : mode == "cancel" ? 300 : mode == "late" ? 4200 : 100;
    Sleep(delay);
    if (mode == "cancel") { SplashScreen::Close({}); log << "cancelled at " << GetTickCount64()-start; return 0; }
    if (mode != "missing") {
        HWND hwnd = OwnSplash();
        while (!hwnd && GetTickCount64() - start < 3000) { Sleep(8); hwnd = OwnSplash(); }
        if (!hwnd || !ValidateWindow(hwnd)) { SplashScreen::Close({}); return 5; }
        RECT client{};
        GetClientRect(hwnd, &client);
        log << "normal window style and monitor-scaled client " << client.right << "x" << client.bottom << " verified" << std::endl;
        if (mode == "user-close") {
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            while (SplashScreen::IsActive() && GetTickCount64()-start < 3000) { Pump(); Sleep(8); }
            SplashScreen::Close({});
            try { SplashScreen::CheckCancellation(); }
            catch (const SplashScreen::Cancelled &) { log << "user close cancels startup" << std::endl; return 0; }
            return 6;
        }
        if (mode == "minimized" || mode == "handoff-minimized") {
            SendMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
            if (!IsIconic(hwnd)) { SplashScreen::Close({}); return 7; }
        }
    }
    if (mode == "abandon") {
        HWND hwnd = Window::Claim();
        if (!hwnd) return 12;
        Window::Abandon(hwnd); DestroyWindow(hwnd);
        UnregisterClassW(SplashScreen::WindowClassName(), GetModuleHandleW(nullptr));
        SplashScreen::Close({}); SplashScreen::CheckCancellation();
        log << "claimed window abandoned safely before startup finished" << std::endl;
        return IsWindow(hwnd) ? 13 : 0;
    }
    const bool handoff = mode.starts_with("handoff");
    const HWND original = OwnSplash();
    if (handoff) {
        if (!original || GetWindowThreadProcessId(original, nullptr) != GetCurrentThreadId() || Window::Claim() != original || Window::Claim()) return 8;
    }
    GameEngine::Ready();
    log << "ready at " << GetTickCount64()-start << std::endl;
    while (SplashScreen::IsActive() && GetTickCount64()-start < delay + 7000) { Pump(); Sleep(8); }
    bool timeout = SplashScreen::IsActive();
    if (handoff && !timeout) {
        GameEngine::Finish();
        if (!IsWindow(original)) return 9;
        Window::Detach(original);
        SetWindowLongPtrW(original, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(GameWindowProc));
        SplashScreen::Close({}); // The splash must never destroy a detached game window.
        if (!IsWindow(original) || SendMessageW(original, WM_APP + 50, 0, 0) != 42) return 10;
        if (mode == "handoff-minimized" && !IsIconic(original)) return 11;
        DestroyWindow(original); UnregisterClassW(SplashScreen::WindowClassName(), GetModuleHandleW(nullptr));
        log << "same HWND and owner thread retained; game procedure active; shutdown safe" << std::endl;
    }
    SplashScreen::Close({});
    log << "closed at " << GetTickCount64()-start << " timeout=" << timeout << std::endl;
    const auto elapsed = GetTickCount64()-start;
    if (custom && elapsed > 2000) return 14; // Custom package selected; invalid intro skipped.
    // Exact animation boundaries have a separate deterministic test.
    if (mode == "early" && elapsed < 3900) return 2;
    if (mode == "late" && elapsed < 5900) return 3;
    if (mode == "missing" && elapsed > 2000) return 4;
    SplashScreen::Close({}); // idempotent shutdown
    SplashScreen::CheckCancellation(); // Natural animation completion is not user cancellation.
    return timeout ? 1 : 0;
}
}
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR args, int) {
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    std::string mode = args;
    const auto end = mode.find_last_not_of(" \t\r\n");
    mode = end == std::string::npos ? "late" : mode.substr(0, end + 1);
    return KashipanEngine::Execute({}, mode);
}
