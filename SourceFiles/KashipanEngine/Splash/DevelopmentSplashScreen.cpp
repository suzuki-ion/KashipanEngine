#if !defined(RELEASE_BUILD)
#include "SplashScreen.h"
#include "Debug/Logger.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <ShlObj.h>

#include <atomic>
#include <thread>

#include <wrl/client.h>
#include <wrl/event.h>
#include <WebView2.h>

#include "Core/ProjectPaths.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/JSON.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace KashipanEngine {
namespace {

/// @brief スプラッシュのクライアント領域サイズ
constexpr int kClientWidth = 640;
constexpr int kClientHeight = 360;

/// @brief UIのHTMLを配置しているフォルダ（エンジンルート基準）
constexpr const char *kUIFolderName = "EngineData/UI/Splash";

/// @brief HTMLの読み込み元として割り当てる仮想ホスト名
constexpr const wchar_t *kVirtualHostName = L"splash.kashipanengine";
constexpr const wchar_t *kStartPageUrl = L"https://splash.kashipanengine/index.html";

/// @brief ログをページへ転送する間隔とタイマーID
constexpr UINT_PTR kLogTimerId = 1, kMinimumTimerId = 2, kCloseTimerId = 3;
constexpr UINT kBeginClosing = WM_APP + 40;
constexpr UINT kLogPollIntervalMs = 100;

/// @brief スプラッシュを最低でもこの時間は表示し続ける
/// @details 初期化が一瞬で終わる環境（キャッシュ済みDevelopment/Release起動等）でも、
///          ロゴが一瞬で消えてしまわないようにするための下限
constexpr DWORD kMinimumDisplayMs = 800;
/// @brief フェードアウト演出（splash.css側のtransitionと合わせる）の長さ
constexpr DWORD kFadeOutMs = 200;

const wchar_t *const kWindowClassName = L"KashipanEngineSplash";

std::thread sThread;
std::atomic<HWND> sHwnd{ nullptr };
ComPtr<ICoreWebView2Controller> sController;
ComPtr<ICoreWebView2> sWebView;
HANDLE sWindowReadyEvent = nullptr;
ULONGLONG sShowStartTick = 0;
std::atomic<bool> sCloseRequested{false}, sRunning{false};
std::atomic<unsigned long long> sGeneration{0};
bool sClosing = false; // WebView の UI スレッド専用。

/// @brief WebView2がキャッシュ等を書き込む作業フォルダ
/// @details ランチャー（KashipanHub）とはユーザーデータフォルダを分け、
///          起動プロセスの入れ替わりが重なってもWebView2側のロックが競合しないようにする
std::wstring GetUserDataFolder() {
    // 診断用に分離したプロファイルを指定できる。既定では従来の保存場所を使う。
    wchar_t overrideFolder[32768]{};
    const DWORD length = GetEnvironmentVariableW(L"KASHIPAN_SPLASH_USER_DATA_FOLDER", overrideFolder, 32768);
    if (length && length < 32768) return overrideFolder;
    PWSTR localAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))) {
        return {};
    }
    std::wstring folder(localAppData);
    CoTaskMemFree(localAppData);
    folder += L"\\KashipanEngine\\SplashWebView";
    return folder;
}

} // namespace

void SplashScreen::NotifyClosing() {
    if (!sWebView) return;
    JSON message = JSON::object();
    message["type"] = "closing";
    sWebView->PostWebMessageAsJson(ConvertString(message.dump()).c_str());
}

void SplashScreen::FlushLogLinesToWebView() {
    if (!sWebView) return;

    std::vector<LogLine> lines = FetchNewSplashLogLines(Passkey<SplashScreen>{});
    if (lines.empty()) return;

    JSON message = JSON::object();
    message["type"] = "log";
    JSON items = JSON::array();
    for (const auto &line : lines) {
        JSON item = JSON::object();
        item["text"] = line.text;
        item["severity"] = static_cast<int>(line.severity);
        items.push_back(std::move(item));
    }
    message["items"] = std::move(items);

    sWebView->PostWebMessageAsJson(ConvertString(message.dump()).c_str());
}

void SplashScreen::CreateWebView(HWND hwnd) {
    const std::wstring userDataFolder = GetUserDataFolder();
    const auto generation = sGeneration.load(std::memory_order_acquire);

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        userDataFolder.empty() ? nullptr : userDataFolder.c_str(),
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd, generation](HRESULT result, ICoreWebView2Environment *environment) -> HRESULT {
                // 失敗してもスプラッシュに何も表示されないだけで、起動自体は継続させる
                if (FAILED(result) || !environment || sCloseRequested.load(std::memory_order_acquire) ||
                    sGeneration.load(std::memory_order_acquire) != generation || sHwnd.load(std::memory_order_acquire) != hwnd) return S_OK;

                environment->CreateCoreWebView2Controller(
                    hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd, generation](HRESULT controllerResult, ICoreWebView2Controller *controller) -> HRESULT {
                            if (FAILED(controllerResult) || !controller) return S_OK;
                            // Close()後にコールバックが遅れて届いた場合は何もしない
                            if (sHwnd.load(std::memory_order_acquire) != hwnd || sGeneration.load(std::memory_order_acquire) != generation ||
                                sCloseRequested.load(std::memory_order_acquire)) return S_OK;

                            sController = controller;
                            if (FAILED(sController->get_CoreWebView2(&sWebView)) || !sWebView) return S_OK;

                            ComPtr<ICoreWebView2Settings> settings;
                            if (SUCCEEDED(sWebView->get_Settings(&settings)) && settings) {
                                settings->put_AreDefaultContextMenusEnabled(FALSE);
                                settings->put_AreDevToolsEnabled(FALSE);
                                settings->put_IsStatusBarEnabled(FALSE);
                                settings->put_IsZoomControlEnabled(FALSE);
                            }

                            ComPtr<ICoreWebView2_3> hostMapping;
                            if (FAILED(sWebView.As(&hostMapping)) || !hostMapping) return S_OK;
                            const std::wstring uiFolder = ConvertString(ProjectPaths::InEngineRoot(kUIFolderName));
                            if (FAILED(hostMapping->SetVirtualHostNameToFolderMapping(
                                kVirtualHostName, uiFolder.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS))) return S_OK;

                            RECT bounds{};
                            GetClientRect(hwnd, &bounds);
                            sController->put_Bounds(bounds);
                            sController->put_IsVisible(TRUE);
                            sWebView->Navigate(kStartPageUrl);

                            // WebViewが使える状態になってから、ログ配信タイマーを開始する
                            SetTimer(hwnd, kLogTimerId, kLogPollIntervalMs, nullptr);
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());
}

LRESULT CALLBACK SplashScreen::WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case kBeginClosing: {
        if (sClosing) return 0;
        sClosing = true;
        const ULONGLONG elapsed = GetTickCount64() - sShowStartTick;
        if (elapsed < kMinimumDisplayMs) {
            if (!SetTimer(window, kMinimumTimerId, static_cast<UINT>(kMinimumDisplayMs - elapsed), nullptr)) DestroyWindow(window);
        } else {
            FlushLogLinesToWebView();
            NotifyClosing(); // COM インターフェイスは、所有する STA スレッドからのみ使用する。
            if (!SetTimer(window, kCloseTimerId, kFadeOutMs, nullptr)) DestroyWindow(window);
        }
        return 0;
    }
    case WM_TIMER:
        if (wparam == kLogTimerId) FlushLogLinesToWebView();
        else if (wparam == kMinimumTimerId) {
            KillTimer(window, kMinimumTimerId);
            FlushLogLinesToWebView(); NotifyClosing();
            if (!SetTimer(window, kCloseTimerId, kFadeOutMs, nullptr)) DestroyWindow(window);
        } else if (wparam == kCloseTimerId) DestroyWindow(window);
        return 0;

    case WM_SIZE:
        if (sController) {
            const RECT bounds{ 0, 0, LOWORD(lparam), HIWORD(lparam) };
            sController->put_Bounds(bounds);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(window, kLogTimerId); KillTimer(window, kMinimumTimerId); KillTimer(window, kCloseTimerId);
        sHwnd.store(nullptr, std::memory_order_release);
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

void SplashScreen::ThreadMain() {
    struct RunningGuard { ~RunningGuard() { sRunning.store(false, std::memory_order_release); } } runningGuard;
    const auto signalReady = [] { if (sWindowReadyEvent) SetEvent(sWindowReadyEvent); };
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) { signalReady(); return; }
    HWND hwnd = nullptr;
    try {
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass); windowClass.lpfnWndProc = WindowProc;
        windowClass.hInstance = GetModuleHandleW(nullptr); windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        windowClass.lpszClassName = kWindowClassName;
        if (RegisterClassExW(&windowClass) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
            hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kWindowClassName, L"KashipanEngine", WS_POPUP,
                (GetSystemMetrics(SM_CXSCREEN) - kClientWidth) / 2, (GetSystemMetrics(SM_CYSCREEN) - kClientHeight) / 2,
                kClientWidth, kClientHeight, nullptr, nullptr, windowClass.hInstance, nullptr);
            if (hwnd) {
                sHwnd.store(hwnd, std::memory_order_release);
                ShowWindow(hwnd, SW_SHOW); UpdateWindow(hwnd);
                signalReady();
                if (sCloseRequested.load(std::memory_order_acquire)) PostMessageW(hwnd, kBeginClosing, 0, 0);
                else CreateWebView(hwnd);
                MSG message{};
                while (GetMessageW(&message, nullptr, 0, 0) > 0) {
                    TranslateMessage(&message); DispatchMessageW(&message);
                }
            }
        }
    } catch (...) { /* スプラッシュ画面の表示に失敗しても、エンジンは起動を続ける。 */ }
    signalReady();
    if (sController) sController->Close();
    sWebView.Reset(); sController.Reset();
    if (hwnd && sHwnd.load(std::memory_order_acquire) == hwnd) DestroyWindow(hwnd);
    sHwnd.store(nullptr, std::memory_order_release);
    UnregisterClassW(kWindowClassName, GetModuleHandleW(nullptr));
    CoUninitialize();
}

void SplashScreen::Show(PasskeyForGameEngineMain) {
    if (sThread.joinable()) return;
    sCloseRequested.store(false, std::memory_order_release);
    sGeneration.fetch_add(1, std::memory_order_acq_rel);
    sClosing = false;
    sShowStartTick = GetTickCount64();
    BeginSplashLogCapture(Passkey<SplashScreen>{});
    sWindowReadyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    try {
        sRunning.store(true, std::memory_order_release);
        sThread = std::thread(&SplashScreen::ThreadMain);
    } catch (...) {
        sRunning.store(false, std::memory_order_release);
        EndSplashLogCapture(Passkey<SplashScreen>{});
    }
    if (sWindowReadyEvent && sThread.joinable()) WaitForSingleObject(sWindowReadyEvent, 3000);
    // タイムアウト後もワーカーがイベントを通知する可能性があるため、スレッドの終了を待つまでイベントを保持する。
    if (!sThread.joinable() && sWindowReadyEvent) { CloseHandle(sWindowReadyEvent); sWindowReadyEvent = nullptr; }
}

void SplashScreen::Close(PasskeyForGameEngineMain) {
    if (!sThread.joinable()) return;
    sCloseRequested.store(true, std::memory_order_release);
    if (HWND hwnd = sHwnd.load(std::memory_order_acquire)) PostMessageW(hwnd, kBeginClosing, 0, 0);
    sThread.join();
    if (sWindowReadyEvent) { CloseHandle(sWindowReadyEvent); sWindowReadyEvent = nullptr; }
    EndSplashLogCapture(Passkey<SplashScreen>{});
}

// 互換性のための窓口。Release 専用の表示や引き継ぎがエディターに影響しないようにする。
bool SplashScreen::IsActive() { return sRunning.load(std::memory_order_acquire); }
SplashScreen::RenderingBackend SplashScreen::GetRenderingBackend() { return RenderingBackend::None; }
void SplashScreen::NotifyReady(Passkey<GameEngine>) {}
void SplashScreen::ConfigureProject(PasskeyForGameEngineMain, const std::string &) {}
void SplashScreen::CheckCancellation() {}
void SplashScreen::PumpMessages() {}
bool SplashScreen::HasStartupWindow() { return false; }
HWND SplashScreen::ClaimWindow(Passkey<Window>) { return nullptr; }
void SplashScreen::DetachWindow(Passkey<Window>, HWND) {}
void SplashScreen::AbandonWindow(Passkey<Window>, HWND) {}
const wchar_t *SplashScreen::WindowClassName() { return kWindowClassName; }
void SplashScreen::Finish(Passkey<GameEngine>) {}
} // namespace KashipanEngine
#endif // !RELEASE_BUILD
