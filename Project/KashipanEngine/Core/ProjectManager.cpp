#include "ProjectManager.h"

#include <algorithm>
#include <filesystem>

#include <shellapi.h>
#pragma comment(lib, "Shell32.lib")

#include "Core/ProjectPaths.h"
#include "Core/UserSettings.h"
#include "Debug/Logger.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/FileIO/TextFile.h"

namespace KashipanEngine {
namespace {

/// @brief 開くプロジェクトが1つも無い場合に自動生成するプロジェクトの名前
constexpr const char *kDefaultProjectName = "NewProject";

/// @brief エディター本体のexeファイル名（プロジェクト切り替え時に起動し直す対象）
constexpr const char *kExecutableFileName = "KashipanEngine.exe";

/// @brief テンプレートの表示名と説明を書いておくファイル名（各テンプレートフォルダ直下・任意）
constexpr const char *kTemplateFileName = "Template.json";

/// @brief GitHubへのプッシュから除外する際にプロジェクトルート直下へ置く .gitignore のファイル名
constexpr const char *kProjectGitignoreFileName = ".gitignore";

/// @brief プロジェクト名に使えない文字（Windowsのファイル名として無効な文字とパス区切り）
constexpr const char *kInvalidProjectNameCharacters = "\\/:*?\"<>|";

} // namespace

bool ProjectManager::IsValidProjectName(const std::string &name, std::string *outErrorMessage) {
    LogScope scope;
    const auto fail = [outErrorMessage](const std::string &message) {
        if (outErrorMessage) *outErrorMessage = message;
        return false;
    };

    if (name.empty()) return fail(Translation("engine.project.name.invalid.empty"));
    if (name.find_first_of(kInvalidProjectNameCharacters) != std::string::npos) {
        return fail(Translation("engine.project.name.invalid.character"));
    }
    // Windowsでは末尾の空白とピリオドがフォルダ名から取り除かれてしまうため許可しない
    if (name.front() == ' ' || name.back() == ' ' || name.back() == '.') {
        return fail(Translation("engine.project.name.invalid.spaceorperiod"));
    }
    return true;
}

bool ProjectManager::ReadProjectFile(const std::string &projectRootPath, ProjectInfo &outInfo) {
    LogScope scope;
    const std::string filePath = projectRootPath + "/" + ProjectPaths::kProjectFileName;
    const JSON json = LoadJSON(filePath);
    if (!json.is_object()) return false;

    outInfo.rootPath = ProjectPaths::NormalizeSeparators(projectRootPath);
    outInfo.name = PathToUtf8String(Utf8StringToPath(outInfo.rootPath).filename());
    outInfo.description = json.value("description", std::string{});
    outInfo.includeInGithubPush = json.value("includeInGithubPush", true);
    outInfo.formatVersion = json.value("formatVersion", kProjectFormatVersion);
    return true;
}

bool ProjectManager::WriteProjectFile(const ProjectInfo &info) {
    LogScope scope;
    JSON json = JSON::object();
    json["formatVersion"] = info.formatVersion;
    json["projectName"] = info.name;
    json["description"] = info.description;
    json["includeInGithubPush"] = info.includeInGithubPush;
    return SaveJSON(json, info.rootPath + "/" + ProjectPaths::kProjectFileName);
}

std::vector<ProjectManager::ProjectInfo> ProjectManager::GetProjectList() {
    LogScope scope;
    std::vector<ProjectInfo> projects;

    std::error_code ec;
    const std::filesystem::path projectsRoot = Utf8StringToPath(ProjectPaths::ProjectsRoot());
    if (!std::filesystem::is_directory(projectsRoot, ec)) return projects;

    for (const auto &entry : std::filesystem::directory_iterator(
             projectsRoot, std::filesystem::directory_options::skip_permission_denied, ec)) {
        if (!entry.is_directory(ec)) continue;
        ProjectInfo info;
        if (!ReadProjectFile(PathToUtf8String(entry.path()), info)) continue;
        projects.push_back(std::move(info));
    }

    std::sort(projects.begin(), projects.end(),
        [](const ProjectInfo &a, const ProjectInfo &b) { return a.name < b.name; });
    return projects;
}

ProjectManager::ProjectInfo ProjectManager::GetActiveProject() {
    LogScope scope;
    ProjectInfo info;
    if (!ReadProjectFile(ProjectPaths::ProjectRoot(), info)) {
        // 配布形態など Project.json が無い場合でも、名前とパスは埋めて返す
        info.rootPath = ProjectPaths::ProjectRoot();
        info.name = ProjectPaths::ProjectName();
    }
    return info;
}

std::vector<ProjectManager::TemplateInfo> ProjectManager::GetTemplateList() {
    LogScope scope;
    std::vector<TemplateInfo> templates;

    std::error_code ec;
    const std::filesystem::path templatesRoot = Utf8StringToPath(ProjectPaths::AssetsTemplateRoot());
    if (!std::filesystem::is_directory(templatesRoot, ec)) return templates;

    for (const auto &entry : std::filesystem::directory_iterator(
             templatesRoot, std::filesystem::directory_options::skip_permission_denied, ec)) {
        if (!entry.is_directory(ec)) continue;

        TemplateInfo info;
        info.name = PathToUtf8String(entry.path().filename());

        // Template.json は任意。無い場合はフォルダ名をそのまま表示名として使う
        const JSON json = LoadJSON(PathToUtf8String(entry.path()) + "/" + kTemplateFileName);
        if (json.is_object()) {
            info.displayName = json.value("displayName", info.name);
            info.description = json.value("description", std::string{});
        } else {
            info.displayName = info.name;
        }
        templates.push_back(std::move(info));
    }

    std::sort(templates.begin(), templates.end(),
        [](const TemplateInfo &a, const TemplateInfo &b) { return a.displayName < b.displayName; });
    return templates;
}

bool ProjectManager::CreateProject(const std::string &name, const std::string &templateName,
    std::string *outErrorMessage) {
    LogScope scope;
    const auto fail = [outErrorMessage](const std::string &message) {
        if (outErrorMessage) *outErrorMessage = message;
        Log(Translation("engine.project.create.failed") + message, LogSeverity::Error);
        return false;
    };

    if (!IsValidProjectName(name, outErrorMessage)) {
        Log(Translation("engine.project.create.failed") + Translation("engine.project.name.invalid") + name, LogSeverity::Error);
        return false;
    }

    std::error_code ec;
    const std::string projectRoot = ProjectPaths::ProjectsRoot() + "/" + name;
    const std::filesystem::path projectRootPath = Utf8StringToPath(projectRoot);
    if (std::filesystem::exists(projectRootPath, ec)) {
        return fail(Translation("engine.project.create.failed.alreadyexists") + name);
    }

    const std::string resolvedTemplateName = templateName.empty() ? kDefaultTemplateName : templateName;
    const std::string templateRootPath = ProjectPaths::AssetsTemplateRoot() + "/" + resolvedTemplateName;
    const std::filesystem::path templateRoot = Utf8StringToPath(templateRootPath);
    if (!std::filesystem::is_directory(templateRoot, ec)) {
        return fail(Translation("engine.project.create.failed.templatenotfound") + templateRootPath);
    }

    //--------- テンプレートをコピーしてAssetsフォルダを作る ---------//
    const std::filesystem::path assetsRoot = projectRootPath / ProjectPaths::kAssetsFolderName;
    std::filesystem::create_directories(assetsRoot, ec);
    if (ec) return fail(Translation("engine.project.create.failed.createfolder") + ec.message());

    std::filesystem::copy(templateRoot, assetsRoot,
        std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        // 中途半端に作られたフォルダが残ると次回の作成も失敗するため片付ける
        std::error_code removeError;
        std::filesystem::remove_all(projectRootPath, removeError);
        return fail(Translation("engine.project.create.failed.copytemplate") + ec.message());
    }

    // Template.json はテンプレート自身の説明であってゲームのデータではないため、コピー先からは取り除く
    std::filesystem::remove(assetsRoot / Utf8StringToPath(kTemplateFileName), ec);

    //--------- プロジェクト定義ファイルを書き出す ---------//
    ProjectInfo info;
    info.name = name;
    info.rootPath = ProjectPaths::NormalizeSeparators(projectRoot);
    info.formatVersion = kProjectFormatVersion;
    if (!WriteProjectFile(info)) {
        std::error_code removeError;
        std::filesystem::remove_all(projectRootPath, removeError);
        return fail(Translation("engine.project.create.failed.writefile") + ProjectPaths::kProjectFileName);
    }

    Log(Translation("engine.project.create.succeeded") + info.rootPath +
        Translation("engine.project.create.succeeded.template") + resolvedTemplateName, LogSeverity::Info);
    return true;
}

bool ProjectManager::DeleteProject(const std::string &name, std::string *outErrorMessage) {
    LogScope scope;
    const auto fail = [outErrorMessage](const std::string &message) {
        if (outErrorMessage) *outErrorMessage = message;
        Log(Translation("engine.project.delete.failed") + message, LogSeverity::Error);
        return false;
    };

    // Project.json を持つフォルダだけを対象にする（無関係なフォルダを消してしまわないため）
    ProjectInfo info;
    if (!ReadProjectFile(ProjectPaths::ProjectsRoot() + "/" + name, info)) {
        return fail(Translation("engine.project.notfound") + name);
    }

    // SHFileOperationW へ渡すパスは区切りが '\\' で、末尾が二重のヌル文字である必要がある
    std::wstring targetPath = ConvertString(info.rootPath);
    std::replace(targetPath.begin(), targetPath.end(), L'/', L'\\');
    targetPath.push_back(L'\0');

    SHFILEOPSTRUCTW operation{};
    operation.wFunc = FO_DELETE;
    operation.pFrom = targetPath.c_str();
    // 取り消せる操作にするためごみ箱へ送る。確認は呼び出し側のUIで済ませている
    operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

    const int result = SHFileOperationW(&operation);
    if (result != 0 || operation.fAnyOperationsAborted) {
        return fail(Translation("engine.project.delete.failed.recyclebin"));
    }

    // 削除したプロジェクトが次回起動時の対象だった場合、指定を消しておく
    if (GetStartupProject() == name) {
        SetStartupProject("");
    }

    Log(Translation("engine.project.delete.succeeded") + info.rootPath, LogSeverity::Info);
    return true;
}

bool ProjectManager::OpenProjectInExplorer(const std::string &name, std::string *outErrorMessage) {
    LogScope scope;

    ProjectInfo info;
    if (!ReadProjectFile(ProjectPaths::ProjectsRoot() + "/" + name, info)) {
        if (outErrorMessage) *outErrorMessage = Translation("engine.project.notfound") + name;
        return false;
    }

    const std::wstring folderPath = ConvertString(info.rootPath);
    const HINSTANCE result = ShellExecuteW(
        nullptr, L"open", folderPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    // ShellExecuteW は成功時に32より大きい値を返す
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        if (outErrorMessage) *outErrorMessage = Translation("engine.project.explorer.open.failed");
        Log(Translation("engine.project.explorer.open.failed.path") + info.rootPath, LogSeverity::Error);
        return false;
    }
    return true;
}

bool ProjectManager::SetIncludeInGithubPush(const std::string &name, bool include, std::string *outErrorMessage) {
    LogScope scope;
    const auto fail = [outErrorMessage](const std::string &message) {
        if (outErrorMessage) *outErrorMessage = message;
        Log(Translation("engine.project.github.push.failed") + message, LogSeverity::Error);
        return false;
    };

    ProjectInfo info;
    if (!ReadProjectFile(ProjectPaths::ProjectsRoot() + "/" + name, info)) {
        return fail(Translation("engine.project.notfound") + name);
    }

    const std::string gitignorePath = info.rootPath + "/" + kProjectGitignoreFileName;
    std::error_code ec;
    if (include) {
        // 除外用の .gitignore を取り除き、通常どおりgitの追跡対象へ戻す
        std::filesystem::remove(Utf8StringToPath(gitignorePath), ec);
    } else {
        // フォルダ内の未追跡ファイルがgitに拾われないようにする
        // （既にコミット済みのファイルの追跡解除はこれだけでは行われない）
        SaveTextFile({ gitignorePath, { "*" } });
        if (!std::filesystem::is_regular_file(Utf8StringToPath(gitignorePath), ec)) {
            return fail(Translation("engine.project.github.push.failed.write") + kProjectGitignoreFileName);
        }
    }

    info.includeInGithubPush = include;
    if (!WriteProjectFile(info)) {
        return fail(Translation("engine.project.github.push.failed.write") + ProjectPaths::kProjectFileName);
    }

    Log(Translation("engine.project.github.push.changed") + name, LogSeverity::Info);
    return true;
}

void ProjectManager::SetStartupProject(const std::string &name) {
    LogScope scope;
    UserSettings::SetString(UserSettings::kLastOpenedProjectKey, name);
}

std::string ProjectManager::GetStartupProject() {
    LogScope scope;
    return UserSettings::GetString(UserSettings::kLastOpenedProjectKey, "");
}

bool ProjectManager::RequestRestartWithProject(const std::string &name) {
    LogScope scope;

    ProjectInfo info;
    if (!ReadProjectFile(ProjectPaths::ProjectsRoot() + "/" + name, info)) {
        Log(Translation("engine.project.restart.failed.notfound") + name, LogSeverity::Error);
        return false;
    }

    // 次回以降ランチャーを介さずに起動された場合もこのプロジェクトが開くようにしておく
    SetStartupProject(name);
    sPendingRestartProjectName_ = name;
    Log(Translation("engine.project.restart.requested") + name, LogSeverity::Info);
    return true;
}

bool ProjectManager::StartEditorProcess(const std::string &projectName, std::string *outErrorMessage) {
    LogScope scope;
    const auto fail = [outErrorMessage](const std::string &message) {
        if (outErrorMessage) *outErrorMessage = message;
        Log(Translation("engine.project.editor.launch.failed") + message, LogSeverity::Error);
        return false;
    };

    const std::string executableDirectory = ProjectPaths::ExecutableDirectory();
    const std::wstring executablePath = ConvertString(executableDirectory + "/" + kExecutableFileName);

    std::error_code ec;
    if (!std::filesystem::is_regular_file(executablePath, ec)) {
        return fail(Translation("engine.project.editor.launch.failed.notfound") + kExecutableFileName);
    }

    // CreateProcessW は第2引数を書き換えることがあるため、変更可能なバッファを渡す
    std::wstring commandLine = L"\"" + executablePath + L"\" " +
        ConvertString(std::string(ProjectPaths::kProjectArgumentName)) + L" \"" +
        ConvertString(projectName) + L"\"";

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};

    const BOOL created = CreateProcessW(
        executablePath.c_str(), commandLine.data(),
        nullptr, nullptr, FALSE, 0, nullptr,
        ConvertString(executableDirectory).c_str(),
        &startupInfo, &processInfo);
    if (!created) {
        return fail(Translation("engine.project.editor.launch.failed.createprocess"));
    }

    // 起動したプロセスの終了を待つ必要はないため、ハンドルは即座に閉じる
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    Log(Translation("engine.project.editor.launch.succeeded") + projectName, LogSeverity::Info);
    return true;
}

bool ProjectManager::LaunchPendingRestart(PasskeyForGameEngineMain) {
    LogScope scope;
    if (sPendingRestartProjectName_.empty()) return false;

    const std::string projectName = sPendingRestartProjectName_;
    sPendingRestartProjectName_.clear();
    return StartEditorProcess(projectName, nullptr);
}

bool ProjectManager::LaunchEditor(const std::string &name, std::string *outErrorMessage) {
    LogScope scope;
    ProjectInfo info;
    if (!ReadProjectFile(ProjectPaths::ProjectsRoot() + "/" + name, info)) {
        if (outErrorMessage) *outErrorMessage = Translation("engine.project.notfound") + name;
        return false;
    }

    // ランチャーを介さずに起動された場合も同じプロジェクトが開くようにしておく
    SetStartupProject(name);
    return StartEditorProcess(name, outErrorMessage);
}

bool ProjectManager::EnsureActiveProject(PasskeyForGameEngineMain) {
    LogScope scope;

    // 配布形態ではexeと同じフォルダのAssetsを使うため、プロジェクトの概念を持ち込まない
    if (ProjectPaths::IsStandalone()) return true;

    if (ProjectPaths::HasActiveProject()) {
        SetStartupProject(ProjectPaths::ProjectName());
        return true;
    }

    // プロジェクトが1つも無い初回起動時は、テンプレートから既定のプロジェクトを作って開く
    Log(Translation("engine.project.default.creating"), LogSeverity::Info);
    if (!CreateProject(kDefaultProjectName, kDefaultTemplateName)) return false;

    ProjectPaths::SetActiveProject(Passkey<ProjectManager>{},
        ProjectPaths::ProjectsRoot() + "/" + kDefaultProjectName);
    SetStartupProject(kDefaultProjectName);
    return true;
}

} // namespace KashipanEngine
