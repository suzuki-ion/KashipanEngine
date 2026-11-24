#pragma once
#include "Utilities/Passkeys.h"
#include <exception>
#include <string>
#include <windows.h>

namespace KashipanEngine {
class GameEngine;
class Window;

/// Release では Direct2D でアニメーションを描画し、エンジンスレッドが所有する HWND をゲームでも再利用する。
/// Development と Debug では独立した WebView2 のログウィンドウを表示し、エンジンの初期化後に閉じる。
class SplashScreen {
public:
    enum class RenderingBackend { None, Hardware, Software };
    class Cancelled final : public std::exception {
    public:
        const char *what() const noexcept override { return "Startup cancelled by user"; }
    };
    static void Show(PasskeyForGameEngineMain);
    static void ConfigureProject(PasskeyForGameEngineMain, const std::string &root);
    /// 起動時のシーンが準備できたら、現在の回転を最後まで再生し、フェードしてから DX12 の表示に切り替える。
    static void NotifyReady(Passkey<GameEngine>);
    static bool IsActive();
    static RenderingBackend GetRenderingBackend();
    /// エンジンスレッド上で、初期化処理を安全に区切れる箇所からのみ呼び出す。
    static void CheckCancellation();
    /// 起動時のメッセージだけを処理する。並列アセットジョブがバッファを保持している間は例外を送出しない。
    static void PumpMessages();
    /// すぐにキャンセルし、スレッドの終了を待つ。失敗時や早期終了時も同様に処理する。
    static void Close(PasskeyForGameEngineMain);
    /// アニメーションやリソースの初期化に失敗しても、ネイティブウィンドウは引き続き利用できる。
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
