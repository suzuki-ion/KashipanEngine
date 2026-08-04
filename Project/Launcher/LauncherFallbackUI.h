#pragma once

#include <string>
#include <vector>

#include "Core/ProjectManager.h"
#include "LauncherUI.h"

namespace Launcher {

/// @brief Win32コントロールだけで組んだランチャー画面
/// @details WebView2ランタイムが無い環境でも必ず起動できるようにするためのフォールバック。
///          見た目は素っ気ないが、できること（一覧・作成・削除・フォルダを開く・起動）は
///          WebView2版とまったく同じにしてある。
class FallbackUI final : public ILauncherUI {
public:
    /// @brief この画面に適したクライアント領域のサイズ（96 DPI 基準）
    static constexpr int kClientWidth = 460;
    static constexpr int kClientHeight = 400;

    explicit FallbackUI(float dpiScale) : dpiScale_(dpiScale) {}

    bool Create(HWND window) override;
    void Resize(int width, int height) override;
    bool HandleCommand(WPARAM wparam) override;

private:
    /// @brief 96 DPI 基準の値を実際のDPIに合わせて拡大する
    int Scaled(int value) const;

    HWND CreateChildControl(const wchar_t *className, const wchar_t *text,
        DWORD style, int x, int y, int width, int height, int controlId);

    void RefreshProjectList();
    void RefreshTemplateList();
    void OpenSelectedProject();
    void RevealSelectedProject();
    void DeleteSelectedProject();
    void CreateNewProject();
    void UpdateStatusForSelection();
    void SetStatusText(const std::wstring &text);
    void ToggleIncludeInGithubPush();
    /// @brief GitHubアップロードのチェックボックスを、選択中のプロジェクトの設定値に合わせる
    void SyncGithubCheckboxToSelection();

    /// @brief 選択中のプロジェクトを取得する（未選択の場合は nullptr）
    const KashipanEngine::ProjectManager::ProjectInfo *GetSelectedProject() const;

    HWND window_ = nullptr;
    HWND projectList_ = nullptr;
    HWND openButton_ = nullptr;
    HWND revealButton_ = nullptr;
    HWND deleteButton_ = nullptr;
    HWND templateCombo_ = nullptr;
    HWND githubCheckbox_ = nullptr;
    HWND newProjectNameEdit_ = nullptr;
    HWND statusLabel_ = nullptr;
    HFONT font_ = nullptr;
    float dpiScale_ = 1.0f;

    /// @brief リストボックスの並びと対応するプロジェクト情報
    std::vector<KashipanEngine::ProjectManager::ProjectInfo> projects_;
    /// @brief コンボボックスの並びと対応するテンプレート情報
    std::vector<KashipanEngine::ProjectManager::TemplateInfo> templates_;
};

} // namespace Launcher
