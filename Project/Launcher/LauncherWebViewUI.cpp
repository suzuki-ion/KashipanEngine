#include "LauncherWebViewUI.h"

#include <filesystem>

#include <ShlObj.h>
#include <wrl/event.h>

#include "Core/ProjectManager.h"
#include "Core/ProjectPaths.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/JSON.h"

using namespace KashipanEngine;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace Launcher {
namespace {

/// @brief UIのHTMLを配置しているフォルダ（エンジンルート基準）
constexpr const char *kUIFolderName = "Launcher/UI";

/// @brief HTMLの読み込み元として割り当てる仮想ホスト名
/// @details file:// で直接開くとローカルファイル扱いになり制約が多いため、
///          実フォルダを仮想的なホストへ割り当ててHTTPSのページとして読み込む
constexpr const wchar_t *kVirtualHostName = L"launcher.kashipanengine";
constexpr const wchar_t *kStartPageUrl = L"https://launcher.kashipanengine/index.html";

/// @brief WebView2がキャッシュ等を書き込む作業フォルダ
/// @details exeの隣は書き込めるとは限らないため、ユーザーのローカルアプリデータ以下を使う
std::wstring GetUserDataFolder() {
    PWSTR localAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))) {
        return {};
    }
    std::wstring folder(localAppData);
    CoTaskMemFree(localAppData);
    folder += L"\\KashipanEngine\\LauncherWebView";
    return folder;
}

} // namespace

bool WebViewUI::IsAvailable() {
    // ランタイムが入っていない環境ではここが失敗するので、ウィンドウを作る前に判定できる
    LPWSTR version = nullptr;
    const HRESULT result = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);
    if (FAILED(result) || !version) return false;
    CoTaskMemFree(version);

    // UIのHTMLが見つからない場合もWebView2版は使えない（真っ白な画面になってしまうため）
    std::error_code ec;
    return std::filesystem::is_directory(
        Utf8StringToPath(ProjectPaths::InEngineRoot(kUIFolderName)), ec);
}

WebViewUI::~WebViewUI() {
    if (controller_) controller_->Close();
}

bool WebViewUI::Create(HWND window) {
    window_ = window;

    const std::wstring userDataFolder = GetUserDataFolder();
    const HRESULT result = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        userDataFolder.empty() ? nullptr : userDataFolder.c_str(),
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT r, ICoreWebView2Environment *environment) {
                return OnEnvironmentCreated(r, environment);
            }).Get());

    // ここで失敗した場合は非同期の完了通知も来ないため、呼び出し元がそのまま失敗を扱う
    return SUCCEEDED(result);
}

HRESULT WebViewUI::OnEnvironmentCreated(HRESULT result, ICoreWebView2Environment *environment) {
    if (FAILED(result) || !environment) {
        NotifyFailure();
        return S_OK;
    }

    const HRESULT createResult = environment->CreateCoreWebView2Controller(
        window_,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this](HRESULT r, ICoreWebView2Controller *controller) {
                return OnControllerCreated(r, controller);
            }).Get());
    if (FAILED(createResult)) NotifyFailure();
    return S_OK;
}

HRESULT WebViewUI::OnControllerCreated(HRESULT result, ICoreWebView2Controller *controller) {
    if (FAILED(result) || !controller) {
        NotifyFailure();
        return S_OK;
    }

    controller_ = controller;
    if (FAILED(controller_->get_CoreWebView2(&webView_)) || !webView_) {
        NotifyFailure();
        return S_OK;
    }

    //--------- ブラウザーらしい要素を落として、アプリの画面らしくする ---------//
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webView_->get_Settings(&settings)) && settings) {
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_IsZoomControlEnabled(FALSE);
    }

    //--------- JavaScriptからのメッセージを受け取る ---------//
    EventRegistrationToken token{};
    webView_->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *args) {
                return OnWebMessageReceived(args);
            }).Get(),
        &token);

    //--------- UIフォルダを仮想ホストへ割り当てて読み込む ---------//
    ComPtr<ICoreWebView2_3> webViewWithHostMapping;
    if (FAILED(webView_.As(&webViewWithHostMapping)) || !webViewWithHostMapping) {
        NotifyFailure();
        return S_OK;
    }
    const std::wstring uiFolder = ConvertString(ProjectPaths::InEngineRoot(kUIFolderName));
    if (FAILED(webViewWithHostMapping->SetVirtualHostNameToFolderMapping(
            kVirtualHostName, uiFolder.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS))) {
        NotifyFailure();
        return S_OK;
    }

    RECT clientRect{};
    GetClientRect(window_, &clientRect);
    controller_->put_Bounds(clientRect);
    controller_->put_IsVisible(TRUE);

    webView_->Navigate(kStartPageUrl);
    return S_OK;
}

HRESULT WebViewUI::OnWebMessageReceived(ICoreWebView2WebMessageReceivedEventArgs *args) {
    LPWSTR rawJson = nullptr;
    if (FAILED(args->get_WebMessageAsJson(&rawJson)) || !rawJson) return S_OK;
    const std::string json = ConvertString(std::wstring(rawJson));
    CoTaskMemFree(rawJson);

    const JSON message = JSON::parse(json, nullptr, false);
    if (!message.is_object()) return S_OK;

    const std::string action = message.value("action", std::string{});
    const std::string name = message.value("name", std::string{});
    std::string errorMessage;

    if (action == "refresh") {
        SendProjectList();
    } else if (action == "create") {
        const std::string templateName = message.value("template", std::string{});
        if (ProjectManager::CreateProject(name, templateName, &errorMessage)) {
            SendProjectList();
            SendStatus("info", "プロジェクトを作成しました。", true);
        } else {
            SendStatus("error", errorMessage);
        }
    } else if (action == "delete") {
        // 確認は画面側のダイアログで済ませている
        if (ProjectManager::DeleteProject(name, &errorMessage)) {
            SendProjectList();
            SendStatus("info", "「" + name + "」をごみ箱へ移動しました。");
        } else {
            SendStatus("error", errorMessage);
        }
    } else if (action == "reveal") {
        if (!ProjectManager::OpenProjectInExplorer(name, &errorMessage)) {
            SendStatus("error", errorMessage);
        }
    } else if (action == "open") {
        if (ProjectManager::LaunchEditor(name, &errorMessage)) {
            // エディターが立ち上がったらランチャーの役目は終わり
            DestroyWindow(window_);
        } else {
            SendStatus("error", errorMessage);
        }
    }
    return S_OK;
}

void WebViewUI::SendProjectList() {
    if (!webView_) return;

    JSON message = JSON::object();
    message["type"] = "projects";
    message["startup"] = ProjectManager::GetStartupProject();

    JSON items = JSON::array();
    for (const auto &project : ProjectManager::GetProjectList()) {
        JSON item = JSON::object();
        item["name"] = project.name;
        item["path"] = project.rootPath;
        item["description"] = project.description;
        items.push_back(std::move(item));
    }
    message["items"] = std::move(items);

    JSON templates = JSON::array();
    for (const auto &projectTemplate : ProjectManager::GetTemplateList()) {
        JSON item = JSON::object();
        item["name"] = projectTemplate.name;
        item["displayName"] = projectTemplate.displayName;
        item["description"] = projectTemplate.description;
        templates.push_back(std::move(item));
    }
    message["templates"] = std::move(templates);

    webView_->PostWebMessageAsJson(ConvertString(message.dump()).c_str());
}

void WebViewUI::SendStatus(const char *level, const std::string &text, bool clearNameInput) {
    if (!webView_) return;

    JSON message = JSON::object();
    message["type"] = "status";
    message["level"] = level;
    message["text"] = text;
    message["clearNameInput"] = clearNameInput;
    webView_->PostWebMessageAsJson(ConvertString(message.dump()).c_str());
}

void WebViewUI::NotifyFailure() {
    if (window_) PostMessageW(window_, kWebViewFailedMessage, 0, 0);
}

void WebViewUI::Resize(int width, int height) {
    if (!controller_) return;
    const RECT bounds{ 0, 0, width, height };
    controller_->put_Bounds(bounds);
}

} // namespace Launcher
