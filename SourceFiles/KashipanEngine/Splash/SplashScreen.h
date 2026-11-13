#pragma once
#include "Utilities/Passkeys.h"
#include <exception>
#include <string>
#include <windows.h>

namespace KashipanEngine {
class GameEngine;
class Window;

/// Release: Direct2D animation with an engine-thread HWND reused by the game.
/// Development/Debug: independent WebView2 log window, closed after engine initialization.
class SplashScreen {
public:
    enum class RenderingBackend { None, Hardware, Software };
    class Cancelled final : public std::exception {
    public:
        const char *what() const noexcept override { return "Startup cancelled by user"; }
    };
    static void Show(PasskeyForGameEngineMain);
    static void ConfigureProject(PasskeyForGameEngineMain, const std::string &root);
    /// Startup scene prepared. Finish the current revolution, then fade before DX12 presentation.
    static void NotifyReady(Passkey<GameEngine>);
    static bool IsActive();
    static RenderingBackend GetRenderingBackend();
    /// Called only at safe initialization boundaries on the engine thread.
    static void CheckCancellation();
    /// Pump only startup messages, without throwing while parallel asset jobs still own buffers.
    static void PumpMessages();
    /// Cancel immediately and join (including failure/early-exit paths).
    static void Close(PasskeyForGameEngineMain);
    /// Native window remains available even after animation/resource initialization fails.
    static bool HasStartupWindow();
    static HWND ClaimWindow(Passkey<Window>);
    static void DetachWindow(Passkey<Window>, HWND hwnd);
    static void AbandonWindow(Passkey<Window>, HWND hwnd);
    static const wchar_t *WindowClassName();
    static void Finish(Passkey<GameEngine>);
private:
#if !defined(RELEASE_BUILD)
    static void ThreadMain();
    static void CreateWebView(HWND hwnd);
    static void FlushLogLinesToWebView();
    static void NotifyClosing();
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
#endif
};
} // namespace KashipanEngine
