#pragma once

#include <string>

#include <wrl/client.h>
#include <WebView2.h>

#include "LauncherUI.h"

namespace Launcher {

/// @brief WebView2でHTML/CSSのUIを表示するランチャー画面
/// @details 画面の実体は Launcher/UI/ 配下のHTMLで、配色はエンジンリファレンスと揃えてある。
///          JavaScriptとのやり取りは postMessage で行い、実際の処理は ProjectManager が担当する。
class WebViewUI final : public ILauncherUI {
public:
    /// @brief この環境でWebView2が使えるか（ランタイムがインストールされているか）を調べる
    /// @details ウィンドウを作る前にどちらのUIを使うか決めるために呼ぶ。
    static bool IsAvailable();

    ~WebViewUI() override;

    bool Create(HWND window) override;
    void Resize(int width, int height) override;

private:
    /// @brief WebView2環境の生成完了（非同期）
    HRESULT OnEnvironmentCreated(HRESULT result, ICoreWebView2Environment *environment);
    /// @brief WebView2コントローラーの生成完了（非同期）
    HRESULT OnControllerCreated(HRESULT result, ICoreWebView2Controller *controller);
    /// @brief JavaScriptからのメッセージ受信
    HRESULT OnWebMessageReceived(ICoreWebView2WebMessageReceivedEventArgs *args);

    /// @brief 初期化に失敗したことをランチャーウィンドウへ知らせる（Win32版へ切り替わる）
    void NotifyFailure();

    /// @brief プロジェクト一覧を画面へ送る
    void SendProjectList();
    /// @brief ステータス行に表示する文言を送る
    /// @param level "info" または "error"（"error" の場合は操作が再度受け付けられる）
    /// @param clearNameInput 新規プロジェクト名の入力欄を空にするか（作成に成功した場合に使う）
    void SendStatus(const char *level, const std::string &text, bool clearNameInput = false);

    HWND window_ = nullptr;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webView_;
};

} // namespace Launcher
