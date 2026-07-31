#pragma once

#include <string>
#include <vector>

#include "Core/ProjectManager.h"
#include "LauncherUI.h"

namespace Launcher {

/// @brief Win32コントロールだけで組んだランチャー画面
/// @details WebView2ランタイムが無い環境でも必ず起動できるようにするためのフォールバック。
///          見た目は素っ気ないが、プロジェクトの一覧・作成・起動はWebView2版と同じことができる。
class FallbackUI final : public ILauncherUI {
public:
    /// @brief この画面に適したクライアント領域のサイズ（96 DPI 基準）
    static constexpr int kClientWidth = 460;
    static constexpr int kClientHeight = 372;

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
    void OpenSelectedProject();
    void CreateNewProject();
    void UpdateStatusForSelection();
    void SetStatusText(const std::wstring &text);

    HWND window_ = nullptr;
    HWND projectList_ = nullptr;
    HWND openButton_ = nullptr;
    HWND newProjectNameEdit_ = nullptr;
    HWND statusLabel_ = nullptr;
    HFONT font_ = nullptr;
    float dpiScale_ = 1.0f;

    /// @brief リストボックスの並びと対応するプロジェクト情報
    std::vector<KashipanEngine::ProjectManager::ProjectInfo> projects_;
};

} // namespace Launcher
