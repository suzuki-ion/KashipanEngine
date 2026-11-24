#if defined(RELEASE_BUILD)
#include "SplashScreen.h"
#include "SplashAnimation.h"
#include "LoadingAnimation.h"
#include "Core/ProjectPaths.h"
#include "Utilities/Conversion/ConvertString.h"
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>
#include <mutex>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace KashipanEngine {
namespace {
using Microsoft::WRL::ComPtr;
constexpr wchar_t kWindowClass[] = L"KashipanEngineStartupLogo";
constexpr DWORD kWindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
constexpr DWORD kWindowExStyle = WS_EX_APPWINDOW;
constexpr LONGLONG kSecond = 1000000000LL, kFramePeriod = kSecond / 60;
std::thread sThread;
std::atomic<bool> sStop{false}, sCancelled{false}, sRunning{false};
std::atomic<LONGLONG> sReadyTick{0};
std::atomic<bool> sPainted{false};
std::atomic<bool> sConfigured{false};
std::mutex sProjectMutex;
std::filesystem::path sProjectFolder;
// ウィンドウを所有するエンジンスレッドからのみアクセスする。描画側に Window のポインターを渡さない。
HWND sWindow = nullptr;
DWORD sOwnerThread = 0;
bool sClaimed = false;
std::atomic<SplashScreen::RenderingBackend> sBackend{SplashScreen::RenderingBackend::None};

LONGLONG NowTicks() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
struct ComApartment {
    bool initialized = false;
    bool Initialize() {
        if (!initialized) initialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
        return initialized;
    }
    ~ComApartment() { if (initialized) CoUninitialize(); }
};
struct WindowState {
    // 最初に宣言することで、アパートメントが終了する前に COM オブジェクトを解放する。
    ComApartment apartment;
    std::filesystem::path folder;
    LONGLONG firstPaintTick = 0, startTick = 0;
    bool closed = false;
    ComPtr<ID2D1Factory> factory;
    ComPtr<IWICImagingFactory> wic;
    ComPtr<IDWriteFactory> write;
    ComPtr<IDWriteTextFormat> loadingFormat;
    ComPtr<IWICBitmap> gearSource, breadSource, textSource;
    ComPtr<ID2D1HwndRenderTarget> target;
    ComPtr<ID2D1Bitmap> gear, bread, text;
    ComPtr<ID2D1SolidColorBrush> brown, fade;
};

// OS の表示倍率が 100% 以外でも、モニターの物理ピクセルを使う。
void SizeForMonitor(HWND hwnd, HMONITOR target, bool center, UINT dpi = 0) {
    MONITORINFO monitor{sizeof(monitor)};
    if (!GetMonitorInfoW(target, &monitor)) return;
    if (!dpi) dpi = GetDpiForWindow(hwnd);
    RECT frame{};
    AdjustWindowRectExForDpi(&frame, kWindowStyle, FALSE, kWindowExStyle, dpi ? dpi : 96);
    const int frameWidth = frame.right - frame.left, frameHeight = frame.bottom - frame.top;
    const auto &area = monitor.rcWork;
    const auto size = SplashAnimation::ChooseClientSize(
        monitor.rcMonitor.right - monitor.rcMonitor.left, monitor.rcMonitor.bottom - monitor.rcMonitor.top,
        area.right - area.left - frameWidth, area.bottom - area.top - frameHeight);
    const int width = size.width + frameWidth, height = size.height + frameHeight;
    RECT previous{};
    GetWindowRect(hwnd, &previous);
    const int x = center ? area.left + (area.right - area.left - width) / 2 :
        std::clamp(static_cast<int>(previous.left), static_cast<int>(area.left), std::max(static_cast<int>(area.left), static_cast<int>(area.right) - width));
    const int y = center ? area.top + (area.bottom - area.top - height) / 2 :
        std::clamp(static_cast<int>(previous.top), static_cast<int>(area.top), std::max(static_cast<int>(area.top), static_cast<int>(area.bottom) - height));
    SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOACTIVATE | SWP_NOZORDER);
}


SplashAnimation::Frame SampleFrame(const WindowState &state) {
    const auto readyTick = sReadyTick.load(std::memory_order_acquire);
    const double seconds = std::max(0.0, (NowTicks() - state.startTick) / double(kSecond));
    const double readyAt = readyTick ? std::max(0.0, (readyTick - state.startTick) / double(kSecond)) : -1.0;
    return SplashAnimation::Sample(seconds, readyAt);
}

bool InitializeFactories(WindowState &state) {
    if (!state.apartment.Initialize()) return false;
    if (!state.factory && FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, state.factory.GetAddressOf()))) return false;
    if (!state.wic && FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(state.wic.GetAddressOf())))) return false;
    if (!state.write && FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown **>(state.write.GetAddressOf())))) return false;
    if (!state.loadingFormat) {
        if (FAILED(state.write->CreateTextFormat(L"Yu Gothic UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 36, L"ja-JP", state.loadingFormat.GetAddressOf()))) return false;
        state.loadingFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        state.loadingFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    return true;
}

bool DecodeBitmap(WindowState &state, const wchar_t *name, ComPtr<IWICBitmap> &bitmap) {
    if (bitmap) return true;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    return SUCCEEDED(state.wic->CreateDecoderFromFilename((state.folder / name).c_str(), nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf())) &&
        SUCCEEDED(decoder->GetFrame(0, frame.GetAddressOf())) &&
        SUCCEEDED(state.wic->CreateFormatConverter(converter.GetAddressOf())) &&
        SUCCEEDED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
            nullptr, 0, WICBitmapPaletteTypeCustom)) &&
        SUCCEEDED(state.wic->CreateBitmapFromSource(converter.Get(), WICBitmapCacheOnLoad, bitmap.ReleaseAndGetAddressOf()));
}

void DiscardDeviceResources(WindowState &state) {
    state.gear.Reset(); state.bread.Reset(); state.text.Reset();
    state.brown.Reset(); state.fade.Reset(); state.target.Reset();
    sBackend.store(SplashScreen::RenderingBackend::None, std::memory_order_release);
}

bool UploadBitmap(ID2D1RenderTarget *target, WindowState &state, IWICBitmap *source,
    float logicalWidth, float logicalHeight, ID2D1Bitmap **destination) {
    const auto pixels = target->GetPixelSize();
    const auto viewport = SplashAnimation::FitViewport(pixels.width, pixels.height);
    const UINT width = std::max(1U, static_cast<UINT>(std::ceil(logicalWidth * viewport.scale)));
    const UINT height = std::max(1U, static_cast<UINT>(std::ceil(logicalHeight * viewport.scale)));
    UINT sourceWidth = 0, sourceHeight = 0;
    if (FAILED(source->GetSize(&sourceWidth, &sourceHeight))) return false;
    ComPtr<IWICBitmapScaler> scaler;
    IWICBitmapSource *upload = source;
    // 物理ピクセルでの表示サイズに一度だけ縮小する。元の大きな PNG を GPU の
    // 2 タップ線形フィルターで繰り返し縮小した場合に生じるエイリアシングを防ぐ。
    if (width < sourceWidth && height < sourceHeight) {
        if (FAILED(state.wic->CreateBitmapScaler(scaler.GetAddressOf())) ||
            FAILED(scaler->Initialize(source, width, height, WICBitmapInterpolationModeFant))) return false;
        upload = scaler.Get();
    }
    const auto properties = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
        D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
    return SUCCEEDED(target->CreateBitmapFromWicBitmap(upload, &properties, destination));
}

bool CreateTargetResources(ID2D1RenderTarget *target, WindowState &state) {
    return UploadBitmap(target, state, state.gearSource.Get(), 512, 512, state.gear.ReleaseAndGetAddressOf()) &&
        UploadBitmap(target, state, state.breadSource.Get(), 512, 512, state.bread.ReleaseAndGetAddressOf()) &&
        UploadBitmap(target, state, state.textSource.Get(), SplashAnimation::kTextWidth, SplashAnimation::kTextHeight, state.text.ReleaseAndGetAddressOf()) &&
        SUCCEEDED(target->CreateSolidColorBrush(D2D1::ColorF(0x654018), state.brown.ReleaseAndGetAddressOf())) &&
        SUCCEEDED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), state.fade.ReleaseAndGetAddressOf()));
}

bool InitializeGraphics(HWND hwnd, WindowState &state, bool softwareOnly = false) {
    if (!InitializeFactories(state) || !DecodeBitmap(state, L"engineLogoGear.png", state.gearSource) ||
        !DecodeBitmap(state, L"engineLogoBread.png", state.breadSource) || !DecodeBitmap(state, L"engineLogoText.png", state.textSource)) return false;
    RECT client{}; GetClientRect(hwnd, &client);
    const auto size = D2D1::SizeU(std::max(1L, client.right), std::max(1L, client.bottom));
    auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_HARDWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96, 96);
    const auto hwndProperties = D2D1::HwndRenderTargetProperties(hwnd, size, D2D1_PRESENT_OPTIONS_IMMEDIATELY);
    auto backend = SplashScreen::RenderingBackend::Hardware;
    HRESULT hr = softwareOnly ? E_FAIL : state.factory->CreateHwndRenderTarget(properties, hwndProperties, state.target.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        properties.type = D2D1_RENDER_TARGET_TYPE_SOFTWARE;
        backend = SplashScreen::RenderingBackend::Software;
        hr = state.factory->CreateHwndRenderTarget(properties, hwndProperties, state.target.ReleaseAndGetAddressOf());
    }
    if (FAILED(hr) || !CreateTargetResources(state.target.Get(), state)) { DiscardDeviceResources(state); return false; }
    state.target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    sBackend.store(backend, std::memory_order_release);
    if (!state.startTick) {
        // 可能な限り、最初の 1 秒間の背景表示中に準備を進める。
        // 準備に時間がかかっても、ロゴが現れ始める部分は飛ばさない。
        state.startTick = std::max(state.firstPaintTick, NowTicks() - kSecond);
    }
    return true;
}

void DrawSprite(ID2D1RenderTarget *target, ID2D1Bitmap *image,
    float x, float y, float width, float height, float rotation = 0) {
    if (!image || width <= 0 || height <= 0) return;
    D2D1_MATRIX_3X2_F previous;
    target->GetTransform(&previous);
    const auto point = SplashAnimation::ToCanvas(x, y);
    target->SetTransform(D2D1::Matrix3x2F::Rotation(SplashAnimation::ToCanvasRotation(rotation)) *
        D2D1::Matrix3x2F::Translation(point.x, point.y) * previous);
    target->DrawBitmap(image, D2D1::RectF(-width / 2, -height / 2, width / 2, height / 2), 1,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    target->SetTransform(previous);
}

void DrawLogoFrame(ID2D1RenderTarget *target, WindowState &state, const SplashAnimation::Frame &frame, int width, int height) {
    using namespace SplashAnimation;
    target->SetTransform(D2D1::Matrix3x2F::Identity());
    target->Clear(D2D1::ColorF(0xF7C052));
    const auto viewport = FitViewport(width, height);
    target->SetTransform(D2D1::Matrix3x2F(viewport.scale, 0, 0, viewport.scale, viewport.x, viewport.y));
    DrawSprite(target, state.gear.Get(), kLogoX, kLogoY, frame.gearScale, frame.gearScale, frame.gearRotation);
    DrawSprite(target, state.bread.Get(), kLogoX, kLogoY, frame.breadScale, frame.breadScale);
    if (frame.textAlpha > 0) DrawSprite(target, state.text.Get(), kLogoX, kTextY, kTextWidth, kTextHeight);
    if (frame.wipeWidth > 0) {
        const float top = ToCanvas(0, kTextY + kTextHeight * 0.5f).y;
        target->FillRectangle(D2D1::RectF(frame.wipeCenter - frame.wipeWidth / 2, top,
            frame.wipeCenter + frame.wipeWidth / 2, top + kTextHeight), state.brown.Get());
    }
    if (frame.loading) {
        constexpr wchar_t text[] = L"読み込み中…";
        target->DrawText(text, ARRAYSIZE(text) - 1, state.loadingFormat.Get(),
            D2D1::RectF(0, kCanvasHeight - 78, kCanvasWidth, kCanvasHeight - 18), state.brown.Get());
    }
    if (frame.fade > 0) {
        target->SetTransform(D2D1::Matrix3x2F::Identity());
        state.fade->SetOpacity(frame.fade);
        target->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(width), static_cast<float>(height)), state.fade.Get());
    }
}

// 描画ワーカーまたは描画の単独回帰テストからのみ呼び出す。
void Paint(HWND hwnd, WindowState &state) {
    RECT client{}; GetClientRect(hwnd, &client);
    const int width = std::max(1L, client.right), height = std::max(1L, client.bottom);
    if (!state.target) return;
    HRESULT hr = S_OK;
    const auto size = state.target->GetPixelSize();
    if (size.width != static_cast<UINT32>(width) || size.height != static_cast<UINT32>(height)) {
        hr = state.target->Resize(D2D1::SizeU(width, height));
        if (SUCCEEDED(hr) && !CreateTargetResources(state.target.Get(), state)) hr = E_FAIL;
    }
    if (SUCCEEDED(hr)) {
        state.target->BeginDraw();
        DrawLogoFrame(state.target.Get(), state, SampleFrame(state), width, height);
        hr = state.target->EndDraw();
    }
    if (FAILED(hr)) DiscardDeviceResources(state);
    else sPainted.store(true, std::memory_order_release);
}

// このウィンドウプロシージャも、後から設定するゲーム用プロシージャも、常にエンジンスレッドで実行する。
LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CLOSE:
        if (hwnd == sWindow) {
            sCancelled.store(true, std::memory_order_release);
            sStop.store(true, std::memory_order_release);
        } else DestroyWindow(hwnd);
        // 描画ワーカーが HWND の描画ターゲットを解放してから破棄する。
        return 0;
    case WM_DPICHANGED:
        SizeForMonitor(hwnd, MonitorFromRect(reinterpret_cast<RECT *>(lparam), MONITOR_DEFAULTTONEAREST), false, LOWORD(wparam));
        return 0;
    case WM_DISPLAYCHANGE:
        SizeForMonitor(hwnd, MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), false);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        if (!sPainted.load(std::memory_order_acquire)) {
            RECT client{}; GetClientRect(hwnd, &client);
            HBRUSH background = CreateSolidBrush(RGB(247, 192, 82));
            FillRect(dc, &client, background); DeleteObject(background);
        }
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_DESTROY:
        if (hwnd == sWindow) {
            sStop.store(true, std::memory_order_release);
            sWindow = nullptr;
            // この時点では任意の起動用 UI なので、WM_QUIT を送らない。
        }
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

void PumpStartupMessages() {
    if (GetCurrentThreadId() != sOwnerThread || !sWindow) return;
    MSG message{};
    for (int count = 0; count < 64 && PeekMessageW(&message, sWindow, 0, 0, PM_REMOVE); ++count) {
        if (message.message == WM_QUIT) { PostQuitMessage(static_cast<int>(message.wParam)); break; }
        TranslateMessage(&message); DispatchMessageW(&message);
        if (!sWindow) break;
    }
}

void JoinRenderer() {
    if (!sThread.joinable()) return;
    // D2D やドライバーの終了処理がネイティブメッセージを送る場合があるため、所有スレッドが応答できる状態を保つ。
    while (sRunning.load(std::memory_order_acquire)) {
        PumpStartupMessages();
        MsgWaitForMultipleObjectsEx(0, nullptr, 2, QS_SENDMESSAGE, MWMO_INPUTAVAILABLE);
    }
    sThread.join();
}

void MessageLoop(HWND hwnd, WindowState &state) {
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer) timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    struct TimerGuard { HANDLE value; ~TimerGuard() { if (value) CloseHandle(value); } } timerGuard{timer};
    auto nextFrame = NowTicks();
    while (!sStop.load(std::memory_order_acquire) && IsWindow(hwnd)) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        const auto now = NowTicks();
        if (now >= nextFrame) {
            const bool finished = state.startTick && SampleFrame(state).finished;
            if (!state.target && !InitializeGraphics(hwnd, state)) break;
            if (!IsIconic(hwnd) && !(state.target->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED)) Paint(hwnd, state);
            // DX12 に引き継げるよう、HWND と最後に描画した黒いフレームを残す。
            if (finished) break;
            nextFrame += kFramePeriod;
            if (nextFrame <= NowTicks()) nextFrame = NowTicks() + kFramePeriod;
        }
        const auto remaining = std::max<LONGLONG>(1, nextFrame - NowTicks());
        LARGE_INTEGER due{}; due.QuadPart = -std::max<LONGLONG>(1, remaining / 100);
        if (timer && SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
            MsgWaitForMultipleObjectsEx(1, &timer, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        } else {
            MsgWaitForMultipleObjectsEx(0, nullptr, static_cast<DWORD>((remaining + 999999) / 1000000), QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        }
    }
}

void ThreadMain(const std::filesystem::path &folder, HWND hwnd, LONGLONG firstPaint) noexcept {
    struct RunningGuard { ~RunningGuard() { sRunning.store(false, std::memory_order_release); } } runningGuard;
    const auto previousDpiContext = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    struct DpiGuard { DPI_AWARENESS_CONTEXT previous; ~DpiGuard() { if (previous) SetThreadDpiAwarenessContext(previous); } } dpiGuard{previousDpiContext};
    try {
        WindowState state; state.folder = folder; state.firstPaintTick = firstPaint;
        while (!sConfigured.load(std::memory_order_acquire) && !sStop.load(std::memory_order_acquire)) Sleep(1);
        if (sStop.load(std::memory_order_acquire)) return;
        std::filesystem::path package;
        { std::lock_guard lock(sProjectMutex); package = sProjectFolder / "StartupAnimation" / "animation.json"; }
        if (std::filesystem::is_regular_file(package)) {
            auto document = LoadingAnimation::Load(package);
            if (document.errors.empty() && state.apartment.Initialize()) {
                LoadingAnimation::Renderer renderer;
                const auto start = NowTicks();
                while (!sStop.load(std::memory_order_acquire) && IsWindow(hwnd)) {
                    const auto ready = sReadyTick.load(std::memory_order_acquire);
                    const auto frame = document.Sample((NowTicks()-start)/double(kSecond), ready ? std::max(0.0,(ready-start)/double(kSecond)) : -1);
                    if (!IsIconic(hwnd)) {
                        if (!renderer.Draw(hwnd,document,frame)) break;
                        sPainted.store(true,std::memory_order_release);
                        sBackend.store(renderer.IsHardware() ? SplashScreen::RenderingBackend::Hardware : SplashScreen::RenderingBackend::Software,std::memory_order_release);
                    }
                    if (frame.finished) return;
                    Sleep(IsIconic(hwnd) ? 16 : 1);
                }
                if (sStop.load(std::memory_order_acquire)) return;
            } else OutputDebugStringW(L"Invalid startup animation package; using engine animation.\n");
        }
        if (InitializeGraphics(hwnd, state)) MessageLoop(hwnd, state);
        // Direct2D、WIC、COM のリソースは、エンジンがスワップチェーンを作成する前にここですべて解放する。
    } catch (...) { /* スプラッシュアニメーションの処理に失敗しても、ウィンドウを再利用し、エンジンの起動を続ける。 */ }
}
} // namespace

void SplashScreen::Show(PasskeyForGameEngineMain) {
    if (sThread.joinable() || sWindow) return;
    sOwnerThread = GetCurrentThreadId(); sClaimed = false;
    sStop.store(false, std::memory_order_release); sCancelled.store(false, std::memory_order_release);
    sReadyTick.store(0, std::memory_order_release); sPainted.store(false, std::memory_order_release);
    sConfigured.store(false, std::memory_order_release);
    sBackend.store(RenderingBackend::None, std::memory_order_release);
    try {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc); wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = kWindowClass;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
        POINT cursor{}; GetCursorPos(&cursor);
        const auto monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
        sWindow = CreateWindowExW(kWindowExStyle, kWindowClass, L"KashipanEngine", kWindowStyle,
            0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
        if (!sWindow) { UnregisterClassW(kWindowClass, wc.hInstance); return; }
        SizeForMonitor(sWindow, monitor, true);
        ShowWindow(sWindow, SW_SHOWNORMAL);
        UpdateWindow(sWindow); // COM や GPU の初期化を始める前に、まず背景を描画する。
        const auto firstPaint = NowTicks();
        const auto folder = Utf8StringToPath(ProjectPaths::InEngineRoot("EngineData/UI/Splash"));
        sRunning.store(true, std::memory_order_release);
        sThread = std::thread(ThreadMain, folder, sWindow, firstPaint);
    } catch (...) { sRunning.store(false, std::memory_order_release); }
}
void SplashScreen::ConfigureProject(PasskeyForGameEngineMain, const std::string &root) {
    { std::lock_guard lock(sProjectMutex); sProjectFolder=Utf8StringToPath(root); }
    sConfigured.store(true,std::memory_order_release);
}
bool SplashScreen::IsActive() { return sRunning.load(std::memory_order_acquire); }
SplashScreen::RenderingBackend SplashScreen::GetRenderingBackend() { return sBackend.load(std::memory_order_acquire); }
void SplashScreen::CheckCancellation() {
    PumpStartupMessages();
    if (sCancelled.load(std::memory_order_acquire)) throw Cancelled{};
}
void SplashScreen::PumpMessages() {
    if (GetCurrentThreadId() != sOwnerThread || !sWindow) return;
    static LONGLONG lastPump = 0;
    const auto now = NowTicks();
    if (now - lastPump < 4000000) return;
    lastPump = now;
    PumpStartupMessages();
}
void SplashScreen::NotifyReady(Passkey<GameEngine>) {
    LONGLONG expected = 0;
    sReadyTick.compare_exchange_strong(expected, NowTicks(), std::memory_order_release);
}
bool SplashScreen::HasStartupWindow() { return GetCurrentThreadId() == sOwnerThread && sWindow; }
const wchar_t *SplashScreen::WindowClassName() { return kWindowClass; }
HWND SplashScreen::ClaimWindow(Passkey<Window>) {
    if (!HasStartupWindow() || sClaimed || !IsWindow(sWindow)) return nullptr;
    sClaimed = true;
    return sWindow;
}
void SplashScreen::DetachWindow(Passkey<Window>, HWND hwnd) {
    if (!sClaimed || !HasStartupWindow() || hwnd != sWindow) return;
    sStop.store(true, std::memory_order_release);
    JoinRenderer();
    CheckCancellation();
    sWindow = nullptr; sClaimed = false;
    // 以降は Window がこの HWND を所有し、ウィンドウの破棄時にウィンドウクラスの登録を解除する。
}
void SplashScreen::AbandonWindow(Passkey<Window>, HWND hwnd) {
    if (!HasStartupWindow() || hwnd != sWindow) return;
    sStop.store(true, std::memory_order_release);
    JoinRenderer();
    sWindow = nullptr; sClaimed = false;
}
void SplashScreen::Finish(Passkey<GameEngine>) {
    JoinRenderer();
    CheckCancellation();
    if (!sClaimed && sWindow) {
        DestroyWindow(sWindow);
        UnregisterClassW(kWindowClass, GetModuleHandleW(nullptr));
    }
}
void SplashScreen::Close(PasskeyForGameEngineMain) {
    sStop.store(true, std::memory_order_release);
    JoinRenderer();
    if (sWindow && !sClaimed) DestroyWindow(sWindow);
    // 引き取ったウィンドウは、例外やキャンセル時も Window が所有する。
    if (!sClaimed) UnregisterClassW(kWindowClass, GetModuleHandleW(nullptr));
}
} // namespace KashipanEngine

#endif // RELEASE_BUILD
