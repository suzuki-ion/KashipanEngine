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
constexpr int kIdRevealButton = 1007;
constexpr int kIdDeleteButton = 1008;
constexpr int kIdTemplateCombo = 1009;
constexpr int kIdGithubCheckbox = 1010;

/// @brief 96 DPI 基準でのレイアウト
namespace Metrics {
constexpr int kMargin = 12;
constexpr int kListTop = 30;
constexpr int kListHeight = 180;
constexpr int kButtonHeight = 26;
constexpr int kRowGap = 10;
constexpr int kCheckboxHeight = 20;
/// @brief コンボボックスの高さは「開いたときの一覧を含む高さ」として扱われる
constexpr int kComboDropHeight = 200;
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

    //--------- 選択中のプロジェクトに対する操作 ---------//
    const int buttonRowTop = kListTop + kListHeight + kRowGap;
    int buttonLeft = kMargin;
    const auto addButton = [&](const wchar_t *label, int width, int id, DWORD style) {
        HWND button = CreateChildControl(L"BUTTON", label, style,
            buttonLeft, buttonRowTop, width, kButtonHeight, id);
        buttonLeft += width + kRowGap;
        return button;
    };
    openButton_ = addButton(L"開く", 84, kIdOpenButton, BS_DEFPUSHBUTTON);
    revealButton_ = addButton(L"フォルダを開く", 122, kIdRevealButton, BS_PUSHBUTTON);
    deleteButton_ = addButton(L"削除", 78, kIdDeleteButton, BS_PUSHBUTTON);
    addButton(L"更新", 78, kIdRefreshButton, BS_PUSHBUTTON);

    //--------- 選択中のプロジェクトのGitHubプッシュ設定 ---------//
    const int githubCheckboxTop = buttonRowTop + kButtonHeight + kRowGap;
    githubCheckbox_ = CreateChildControl(L"BUTTON", L"GitHubへのプッシュに含める", BS_AUTOCHECKBOX,
        kMargin, githubCheckboxTop, contentWidth, kCheckboxHeight, kIdGithubCheckbox);

    //--------- 新規作成 ---------//
    const int newProjectLabelTop = githubCheckboxTop + kCheckboxHeight + kRowGap * 2;
    CreateChildControl(L"STATIC", L"新規プロジェクト", 0,
        kMargin, newProjectLabelTop, contentWidth, 18, 0);

    const int newProjectRowTop = newProjectLabelTop + 22;
    constexpr int kComboWidth = 140;
    constexpr int kCreateWidth = 94;
    const int editWidth = contentWidth - kComboWidth - kCreateWidth - kRowGap * 2;
    templateCombo_ = CreateChildControl(L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_VSCROLL,
        kMargin, newProjectRowTop, kComboWidth, kComboDropHeight, kIdTemplateCombo);
    newProjectNameEdit_ = CreateChildControl(L"EDIT", nullptr, ES_AUTOHSCROLL,
        kMargin + kComboWidth + kRowGap, newProjectRowTop, editWidth, kButtonHeight, kIdNewProjectNameEdit);
    CreateChildControl(L"BUTTON", L"作成", BS_PUSHBUTTON,
        kMargin + kComboWidth + editWidth + kRowGap * 2, newProjectRowTop,
        kCreateWidth, kButtonHeight, kIdCreateButton);

    statusLabel_ = CreateChildControl(L"STATIC", L"",
        SS_PATHELLIPSIS, kMargin, newProjectRowTop + kButtonHeight + kRowGap,
        contentWidth, 18, kIdStatusLabel);

    RefreshTemplateList();
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
    case kIdRevealButton:
        RevealSelectedProject();
        return true;
    case kIdDeleteButton:
        DeleteSelectedProject();
        return true;
    case kIdRefreshButton:
        RefreshTemplateList();
        RefreshProjectList();
        return true;
    case kIdCreateButton:
        CreateNewProject();
        return true;
    case kIdGithubCheckbox:
        if (HIWORD(wparam) == BN_CLICKED) ToggleIncludeInGithubPush();
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

    const BOOL hasProjects = projects_.empty() ? FALSE : TRUE;
    EnableWindow(openButton_, hasProjects);
    EnableWindow(revealButton_, hasProjects);
    EnableWindow(deleteButton_, hasProjects);
    EnableWindow(githubCheckbox_, hasProjects);

    if (projects_.empty()) {
        SetStatusText(L"プロジェクトがありません。下の欄から新規作成してください。");
        Button_SetCheck(githubCheckbox_, BST_UNCHECKED);
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
    SyncGithubCheckboxToSelection();
    SetStatusText(L"");
}

const ProjectManager::ProjectInfo *FallbackUI::GetSelectedProject() const {
    const int selectedIndex = ListBox_GetCurSel(projectList_);
    if (selectedIndex == LB_ERR || static_cast<size_t>(selectedIndex) >= projects_.size()) return nullptr;
    return &projects_[static_cast<size_t>(selectedIndex)];
}

void FallbackUI::RefreshTemplateList() {
    templates_ = ProjectManager::GetTemplateList();

    ComboBox_ResetContent(templateCombo_);
    int defaultIndex = 0;
    for (size_t i = 0; i < templates_.size(); ++i) {
        ComboBox_AddString(templateCombo_, ConvertString(templates_[i].displayName).c_str());
        if (templates_[i].name == ProjectManager::kDefaultTemplateName) {
            defaultIndex = static_cast<int>(i);
        }
    }
    if (!templates_.empty()) ComboBox_SetCurSel(templateCombo_, defaultIndex);
}

void FallbackUI::OpenSelectedProject() {
    const ProjectManager::ProjectInfo *project = GetSelectedProject();
    if (!project) {
        SetStatusText(L"開くプロジェクトを選択してください。");
        return;
    }

    SetStatusText(L"エディターを起動しています...");

    std::string errorMessage;
    if (!ProjectManager::LaunchEditor(project->name, &errorMessage)) {
        SetStatusText(ConvertString(errorMessage));
        return;
    }

    // エディターが立ち上がったらランチャーの役目は終わり
    DestroyWindow(window_);
}

void FallbackUI::RevealSelectedProject() {
    const ProjectManager::ProjectInfo *project = GetSelectedProject();
    if (!project) {
        SetStatusText(L"フォルダを開くプロジェクトを選択してください。");
        return;
    }

    std::string errorMessage;
    if (!ProjectManager::OpenProjectInExplorer(project->name, &errorMessage)) {
        SetStatusText(ConvertString(errorMessage));
    }
}

void FallbackUI::DeleteSelectedProject() {
    const ProjectManager::ProjectInfo *project = GetSelectedProject();
    if (!project) {
        SetStatusText(L"削除するプロジェクトを選択してください。");
        return;
    }

    // 取り返しのつかない操作なので、既定の選択は「いいえ」にしておく
    const std::wstring message =
        L"「" + ConvertString(project->name) + L"」を削除しますか？\n\n" +
        ConvertString(project->rootPath) + L"\n\n" +
        L"フォルダはごみ箱へ移動します。";
    const int answer = MessageBoxW(window_, message.c_str(), L"プロジェクトの削除",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (answer != IDYES) return;

    const std::wstring deletedName = ConvertString(project->name);
    std::string errorMessage;
    if (!ProjectManager::DeleteProject(project->name, &errorMessage)) {
        SetStatusText(ConvertString(errorMessage));
        return;
    }

    RefreshProjectList();
    SetStatusText(L"「" + deletedName + L"」をごみ箱へ移動しました。");
}

void FallbackUI::CreateNewProject() {
    const int length = GetWindowTextLengthW(newProjectNameEdit_);
    if (length <= 0) {
        SetStatusText(L"プロジェクト名を入力してください。");
        return;
    }

    std::wstring name(static_cast<size_t>(length), L'\0');
    GetWindowTextW(newProjectNameEdit_, name.data(), length + 1);

    // コンボボックスで選ばれているテンプレートを使う（未選択なら既定のテンプレート）
    const int templateIndex = ComboBox_GetCurSel(templateCombo_);
    const std::string templateName =
        (templateIndex != CB_ERR && static_cast<size_t>(templateIndex) < templates_.size())
        ? templates_[static_cast<size_t>(templateIndex)].name
        : std::string{};

    std::string errorMessage;
    if (!ProjectManager::CreateProject(ConvertString(name), templateName, &errorMessage)) {
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
    SyncGithubCheckboxToSelection();
    SetStatusText(L"プロジェクトを作成しました。");
}

void FallbackUI::UpdateStatusForSelection() {
    const int selectedIndex = ListBox_GetCurSel(projectList_);
    if (selectedIndex == LB_ERR || static_cast<size_t>(selectedIndex) >= projects_.size()) return;
    SetStatusText(ConvertString(projects_[selectedIndex].rootPath));
    SyncGithubCheckboxToSelection();
}

void FallbackUI::SyncGithubCheckboxToSelection() {
    const ProjectManager::ProjectInfo *project = GetSelectedProject();
    Button_SetCheck(githubCheckbox_, (project && project->includeInGithubPush) ? BST_CHECKED : BST_UNCHECKED);
}

void FallbackUI::ToggleIncludeInGithubPush() {
    const ProjectManager::ProjectInfo *project = GetSelectedProject();
    if (!project) return;
    const std::string name = project->name;
    const bool include = Button_GetCheck(githubCheckbox_) == BST_CHECKED;

    std::string errorMessage;
    const bool succeeded = ProjectManager::SetIncludeInGithubPush(name, include, &errorMessage);

    // 失敗時もチェックボックスの表示を実際の保存値へ揃え直すため、常に一覧を読み直す
    // （RefreshProjectListは末尾でステータス表示も上書きするため、エラー表示はその後に行う）
    RefreshProjectList();
    for (size_t i = 0; i < projects_.size(); ++i) {
        if (projects_[i].name != name) continue;
        ListBox_SetCurSel(projectList_, static_cast<int>(i));
        break;
    }
    SyncGithubCheckboxToSelection();
    if (!succeeded) SetStatusText(ConvertString(errorMessage));
}

} // namespace Launcher
