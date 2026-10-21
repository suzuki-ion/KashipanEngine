#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace Launcher {

/// @brief ランチャーウィンドウがUIの差し替えを通知されるためのメッセージ
/// @details WebView2の初期化は非同期のため、ウィンドウ生成後に失敗が判明することがある。
///          その場合にWin32版のUIへ切り替えるためのトリガーとして使う。
constexpr UINT kWebViewFailedMessage = WM_APP + 1;

/// @brief ランチャーの画面実装が満たすインターフェース
/// @details HTMLで描画するWebView2版と、Win32コントロールで組んだフォールバック版がある。
///          どちらも表示と入力の受け渡しだけを行い、プロジェクトの列挙・作成・起動は
///          KashipanEngine::ProjectManager に任せる。
class ILauncherUI {
public:
    virtual ~ILauncherUI() = default;

    /// @brief 画面を組み立てる
    /// @param window 親となるランチャーウィンドウ
    /// @return 使用可能な状態になった場合は true
    virtual bool Create(HWND window) = 0;

    /// @brief クライアント領域のサイズ変更を反映する
    virtual void Resize(int width, int height) = 0;

    /// @brief WM_COMMAND を処理する
    /// @return 処理した場合は true
    virtual bool HandleCommand(WPARAM /*wparam*/) { return false; }
};

} // namespace Launcher
