#include "LauncherFallbackUI.h"

#include <windowsx.h>

#include "Utilities/Conversion/ConvertString.h"

using namespace KashipanEngine;

namespace Launcher {
namespace {

//==================================================
// コントロールID
//==================================================
constexpr int kIdProjectList = 1001;
constexpr int kIdOpenButton = 1002;
constexpr int kIdRefreshButton = 1003;
constexpr int kIdNewProjectNameEdit = 1004;
constexpr int kIdCreateButton = 1005;
constexpr int kIdStatusLabel = 1006;

/// @brief 96 DPI 基準でのレイアウト
namespace Metrics {
constexpr int kMargin = 12;
constexpr int kListTop = 30;
constexpr int kListHeight = 190;
constexpr int kButtonWidth = 104;
constexpr int kButtonHeight = 26;
constexpr int kRowGap = 10;
}

/// @brief ウィンドウのメッセージフォント（Yu Gothic UI等）を取得する
HFONT CreateMessageFont(UINT dpi) {
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    if (!SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi)) {
        return static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }
    return CreateFontIndirectW(&metrics.lfMessageFont);
}

} // namespace

int FallbackUI::Scaled(int value) const {
    return static_cast<int>(static_cast<float>(value) * dpiScale_ + 0.5f);
}

HWND FallbackUI::CreateChildControl(const wchar_t *className, const wchar_t *text,
    DWORD style, int x, int y, int width, int height, int controlId) {
    const bool hasClientEdge = (wcscmp(className, L"LISTBOX") == 0 || wcscmp(className, L"EDIT") == 0);
    HWND control = CreateWindowExW(
        hasClientEdge ? WS_EX_CLIENTEDGE : 0,
        className, text, WS_CHILD | WS_VISIBLE | style,
        Scaled(x), Scaled(y), Scaled(width), Scaled(height),
        window_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
        GetModuleHandleW(nullptr), nullptr);
    if (control) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
    return control;
}

bool FallbackUI::Create(HWND window) {
    using namespace Metrics;
    window_ = window;
    font_ = CreateMessageFont(GetDpiForSystem());

    const int contentWidth = kClientWidth - kMargin * 2;

    CreateChildControl(L"STATIC", L"プロジェクトを選択してください", 0,
        kMargin, kMargin, contentWidth, 18, 0);

    projectList_ = CreateChildControl(L"LISTBOX", nullptr,
        LBS_NOTIFY | WS_VSCROLL | LBS_HASSTRINGS,
        kMargin, kListTop, contentWidth, kListHeight, kIdProjectList);

    const int buttonRowTop = kListTop + kListHeight + kRowGap;
    openButton_ = CreateChildControl(L"BUTTON", L"開く", BS_DEFPUSHBUTTON,
        kMargin, buttonRowTop, kButtonWidth, kButtonHeight, kIdOpenButton);
    CreateChildControl(L"BUTTON", L"更新", BS_PUSHBUTTON,
        kMargin + kButtonWidth + kRowGap, buttonRowTop, kButtonWidth, kButtonHeight, kIdRefreshButton);

    const int newProjectLabelTop = buttonRowTop + kButtonHeight + kRowGap * 2;
    CreateChildControl(L"STATIC", L"新規プロジェクト名", 0,
        kMargin, newProjectLabelTop, contentWidth, 18, 0);

    const int newProjectRowTop = newProjectLabelTop + 22;
    const int editWidth = contentWidth - kButtonWidth - kRowGap;
    newProjectNameEdit_ = CreateChildControl(L"EDIT", nullptr, ES_AUTOHSCROLL,
        kMargin, newProjectRowTop, editWidth, kButtonHeight, kIdNewProjectNameEdit);
    CreateChildControl(L"BUTTON", L"作成", BS_PUSHBUTTON,
        kMargin + editWidth + kRowGap, newProjectRowTop, kButtonWidth, kButtonHeight, kIdCreateButton);

    statusLabel_ = CreateChildControl(L"STATIC", L"",
        SS_PATHELLIPSIS, kMargin, newProjectRowTop + kButtonHeight + kRowGap,
        contentWidth, 18, kIdStatusLabel);

    RefreshProjectList();
    return true;
}

void FallbackUI::Resize(int /*width*/, int /*height*/) {
    // 固定レイアウトのため、サイズ変更に追従する必要はない
}

bool FallbackUI::HandleCommand(WPARAM wparam) {
    switch (LOWORD(wparam)) {
    case kIdOpenButton:
        OpenSelectedProject();
        return true;
    case kIdRefreshButton:
        RefreshProjectList();
        return true;
    case kIdCreateButton:
        CreateNewProject();
        return true;
    case kIdProjectList:
        // ダブルクリックはそのまま「開く」として扱う
        if (HIWORD(wparam) == LBN_DBLCLK) {
            OpenSelectedProject();
        } else if (HIWORD(wparam) == LBN_SELCHANGE) {
            UpdateStatusForSelection();
        }
        return true;
    default:
        return false;
    }
}

void FallbackUI::SetStatusText(const std::wstring &text) {
    SetWindowTextW(statusLabel_, text.c_str());
}

void FallbackUI::RefreshProjectList() {
    projects_ = ProjectManager::GetProjectList();

    ListBox_ResetContent(projectList_);
    for (const auto &project : projects_) {
        ListBox_AddString(projectList_, ConvertString(project.name).c_str());
    }

    if (projects_.empty()) {
        SetStatusText(L"プロジェクトがありません。下の欄から新規作成してください。");
        EnableWindow(openButton_, FALSE);
        return;
    }

    // 前回開いたプロジェクトがあれば、それを初期選択にする
    const std::string startupProject = ProjectManager::GetStartupProject();
    int selectedIndex = 0;
    for (size_t i = 0; i < projects_.size(); ++i) {
        if (projects_[i].name == startupProject) {
            selectedIndex = static_cast<int>(i);
            break;
        }
    }
    ListBox_SetCurSel(projectList_, selectedIndex);
    EnableWindow(openButton_, TRUE);
    SetStatusText(L"");
}

void FallbackUI::OpenSelectedProject() {
    const int selectedIndex = ListBox_GetCurSel(projectList_);
    if (selectedIndex == LB_ERR || static_cast<size_t>(selectedIndex) >= projects_.size()) {
        SetStatusText(L"開くプロジェクトを選択してください。");
        return;
    }

    SetStatusText(L"エディターを起動しています...");

    std::string errorMessage;
    if (!ProjectManager::LaunchEditor(projects_[selectedIndex].name, &errorMessage)) {
        SetStatusText(ConvertString(errorMessage));
        return;
    }

    // エディターが立ち上がったらランチャーの役目は終わり
    DestroyWindow(window_);
}

void FallbackUI::CreateNewProject() {
    const int length = GetWindowTextLengthW(newProjectNameEdit_);
    if (length <= 0) {
        SetStatusText(L"プロジェクト名を入力してください。");
        return;
    }

    std::wstring name(static_cast<size_t>(length), L'\0');
    GetWindowTextW(newProjectNameEdit_, name.data(), length + 1);

    std::string errorMessage;
    if (!ProjectManager::CreateProject(ConvertString(name), &errorMessage)) {
        SetStatusText(ConvertString(errorMessage));
        return;
    }

    SetWindowTextW(newProjectNameEdit_, L"");
    RefreshProjectList();

    // 作成したプロジェクトを選択状態にして、そのまま開けるようにする
    const std::string createdName = ConvertString(name);
    for (size_t i = 0; i < projects_.size(); ++i) {
        if (projects_[i].name != createdName) continue;
        ListBox_SetCurSel(projectList_, static_cast<int>(i));
        break;
    }
    SetStatusText(L"プロジェクトを作成しました。");
}

void FallbackUI::UpdateStatusForSelection() {
    const int selectedIndex = ListBox_GetCurSel(projectList_);
    if (selectedIndex == LB_ERR || static_cast<size_t>(selectedIndex) >= projects_.size()) return;
    SetStatusText(ConvertString(projects_[selectedIndex].rootPath));
}

} // namespace Launcher
