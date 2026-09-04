#include "SplashScreen.h"

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
#include "Debug/Logger.h"
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
constexpr const char *kUIFolderName = "KashipanEngine/Splash/UI";

/// @brief HTMLの読み込み元として割り当てる仮想ホスト名
constexpr const wchar_t *kVirtualHostName = L"splash.kashipanengine";
constexpr const wchar_t *kStartPageUrl = L"https://splash.kashipanengine/index.html";

/// @brief ログをページへ転送する間隔とタイマーID
constexpr UINT_PTR kLogTimerId = 1;
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

/// @brief WebView2がキャッシュ等を書き込む作業フォルダ
/// @details ランチャー（KashipanHub）とはユーザーデータフォルダを分け、
///          起動プロセスの入れ替わりが重なってもWebView2側のロックが競合しないようにする
std::wstring GetUserDataFolder() {
    LogScope scope;
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
    LogScope scope;
    if (!sWebView) return;
    JSON message = JSON::object();
    message["type"] = "closing";
    sWebView->PostWebMessageAsJson(ConvertString(message.dump()).c_str());
}

void SplashScreen::FlushLogLinesToWebView() {
    LogScope scope;
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
    LogScope scope;
    const std::wstring userDataFolder = GetUserDataFolder();

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        userDataFolder.empty() ? nullptr : userDataFolder.c_str(),
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd](HRESULT result, ICoreWebView2Environment *environment) -> HRESULT {
                // 失敗してもスプラッシュに何も表示されないだけで、起動自体は継続させる
                if (FAILED(result) || !environment) return S_OK;

                environment->CreateCoreWebView2Controller(
                    hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd](HRESULT controllerResult, ICoreWebView2Controller *controller) -> HRESULT {
                            if (FAILED(controllerResult) || !controller) return S_OK;
                            // Close()後にコールバックが遅れて届いた場合は何もしない
                            if (sHwnd.load(std::memory_order_acquire) != hwnd) return S_OK;

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
                            if (SUCCEEDED(sWebView.As(&hostMapping)) && hostMapping) {
                                const std::wstring uiFolder = ConvertString(ProjectPaths::InEngineRoot(kUIFolderName));
                                hostMapping->SetVirtualHostNameToFolderMapping(
                                    kVirtualHostName, uiFolder.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
                            }

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
    LogScope scope;
    switch (message) {
    case WM_TIMER:
        if (wparam == kLogTimerId) {
            FlushLogLinesToWebView();
        }
        return 0;

    case WM_SIZE:
        if (sController) {
            const RECT bounds{ 0, 0, LOWORD(lparam), HIWORD(lparam) };
            sController->put_Bounds(bounds);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(window, kLogTimerId);
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

void SplashScreen::ThreadMain() {
    LogScope scope;
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
        SetEvent(sWindowReadyEvent);
        return;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    windowClass.lpszClassName = kWindowClassName;
    RegisterClassExW(&windowClass);

    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    const int x = (screenWidth - kClientWidth) / 2;
    const int y = (screenHeight - kClientHeight) / 2;

    // 枠なし（WS_POPUP）にして、Unity等と同様のスプラッシュらしい見た目にする。
    // タスクバーには出さない（WS_EX_TOOLWINDOW）
    HWND hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        kWindowClassName, L"KashipanEngine",
        WS_POPUP,
        x, y, kClientWidth, kClientHeight,
        nullptr, nullptr, windowClass.hInstance, nullptr);

    if (!hwnd) {
        SetEvent(sWindowReadyEvent);
        CoUninitialize();
        return;
    }

    sHwnd.store(hwnd, std::memory_order_release);
    SetEvent(sWindowReadyEvent);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    CreateWebView(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    sController.Reset();
    sWebView.Reset();
    sHwnd.store(nullptr, std::memory_order_release);
    CoUninitialize();
}

void SplashScreen::Show(PasskeyForGameEngineMain) {
    LogScope scope;
    if (sThread.joinable()) return;

    BeginSplashLogCapture(Passkey<SplashScreen>{});
    sShowStartTick = GetTickCount64();

    sWindowReadyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    sThread = std::thread(&SplashScreen::ThreadMain);

    // ウィンドウの生成完了（に相当するタイミング）まではここで待つ。
    // GameEngineの重い初期化が始まる前にスプラッシュを画面へ出しておきたいための同期であり、
    // WebView2自体の初期化・描画は待たない（そちらは別スレッドで非同期に進む）
    if (sWindowReadyEvent) {
        WaitForSingleObject(sWindowReadyEvent, 3000);
        CloseHandle(sWindowReadyEvent);
        sWindowReadyEvent = nullptr;
    }
}

void SplashScreen::Close(PasskeyForGameEngineMain) {
    LogScope scope;
    if (!sThread.joinable()) return;

    // 初期化が一瞬で終わった場合でも、ロゴが一瞬で消えないよう最低表示時間まで待つ
    const ULONGLONG elapsedMs = GetTickCount64() - sShowStartTick;
    if (elapsedMs < kMinimumDisplayMs) {
        Sleep(static_cast<DWORD>(kMinimumDisplayMs - elapsedMs));
    }

    // フェードアウト演出を開始させてから、演出時間分だけ待って閉じる
    NotifyClosing();
    Sleep(kFadeOutMs);

    if (HWND hwnd = sHwnd.load(std::memory_order_acquire)) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }
    sThread.join();

    EndSplashLogCapture(Passkey<SplashScreen>{});
}

} // namespace KashipanEngine
