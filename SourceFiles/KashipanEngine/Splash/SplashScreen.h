#pragma once
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

/// @brief エンジン起動中に表示するスプラッシュ画面
/// @details GameEngineの初期化（Window/DirectX12/各種マネージャ生成）はメインスレッドで
///          同期的に行われるため、スプラッシュ画面自体は専用スレッドでWebView2を独立して
///          動かすことで、初期化中も見た目が固まらないようにしている。
///          表示中はLoggerの出力を購読し、流れるログとしてページへ転送する（Logger::BeginSplashLogCapture等）。
///          DirectX12の初期化より前から表示できるよう、エンジン本体のWindow/WindowsAPIには
///          依存しない生のWin32ウィンドウ + WebView2で実装している（KashipanHubのランチャー画面と同じ方式）。
class SplashScreen {
public:
    /// @brief スプラッシュ画面の表示を開始する
    /// @details ウィンドウの生成までは呼び出し元をブロックするが、
    ///          WebView2の初期化・描画は専用スレッドで非同期に進む。
    static void Show(PasskeyForGameEngineMain);

    /// @brief スプラッシュ画面を閉じ、専用スレッドの終了を待つ（ブロッキング）
    static void Close(PasskeyForGameEngineMain);

private:
    // 以下はすべて専用スレッド上でのみ呼ばれる内部処理。
    // Logger::FetchNewSplashLogLines等がPasskey<SplashScreen>を要求し、
    // それを構築できるのはこのクラスのメンバーだけのためメンバー関数にしている。

    /// @brief 専用スレッドの本体（ウィンドウ生成〜メッセージループ）
    static void ThreadMain();
    /// @brief WebView2環境の生成を開始する（完了は非同期コールバックで通知される）
    static void CreateWebView(HWND hwnd);
    /// @brief 新しく出力されたログ行をWebViewへ転送する
    static void FlushLogLinesToWebView();
    /// @brief スプラッシュウィンドウのウィンドウプロシージャ
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    /// @brief ページ側へフェードアウト演出の開始を通知する
    static void NotifyClosing();
};

} // namespace KashipanEngine
