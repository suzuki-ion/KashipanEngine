#include "ProjectManager.h"

#include <algorithm>
#include <filesystem>

#include "Core/ProjectPaths.h"
#include "Core/UserSettings.h"
#include "Debug/Logger.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {
namespace {

/// @brief 開くプロジェクトが1つも無い場合に自動生成するプロジェクトの名前
constexpr const char *kDefaultProjectName = "NewProject";

/// @brief エディター本体のexeファイル名（プロジェクト切り替え時に起動し直す対象）
constexpr const char *kExecutableFileName = "KashipanEngine.exe";

/// @brief プロジェクト名に使えない文字（Windowsのファイル名として無効な文字とパス区切り）
constexpr const char *kInvalidProjectNameCharacters = "\\/:*?\"<>|";

} // namespace

bool ProjectManager::IsValidProjectName(const std::string &name, std::string *outErrorMessage) {
    const auto fail = [outErrorMessage](const char *message) {
        if (outErrorMessage) *outErrorMessage = message;
        return false;
    };

    if (name.empty()) return fail("Project name is empty.");
    if (name.find_first_of(kInvalidProjectNameCharacters) != std::string::npos) {
        return fail("Project name contains characters that cannot be used in a folder name.");
    }
    // Windowsでは末尾の空白とピリオドがフォルダ名から取り除かれてしまうため許可しない
    if (name.front() == ' ' || name.back() == ' ' || name.back() == '.') {
        return fail("Project name cannot start or end with a space, or end with a period.");
    }
    return true;
}

bool ProjectManager::ReadProjectFile(const std::string &projectRootPath, ProjectInfo &outInfo) {
    const std::string filePath = projectRootPath + "/" + ProjectPaths::kProjectFileName;
    const JSON json = LoadJSON(filePath);
    if (!json.is_object()) return false;

    outInfo.rootPath = ProjectPaths::NormalizeSeparators(projectRootPath);
    outInfo.name = PathToUtf8String(Utf8StringToPath(outInfo.rootPath).filename());
    outInfo.description = json.value("description", std::string{});
    outInfo.formatVersion = json.value("formatVersion", kProjectFormatVersion);
    return true;
}

bool ProjectManager::WriteProjectFile(const ProjectInfo &info) {
    JSON json = JSON::object();
    json["formatVersion"] = info.formatVersion;
    json["projectName"] = info.name;
    json["description"] = info.description;
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
    ProjectInfo info;
    if (!ReadProjectFile(ProjectPaths::ProjectRoot(), info)) {
        // 配布形態など Project.json が無い場合でも、名前とパスは埋めて返す
        info.rootPath = ProjectPaths::ProjectRoot();
        info.name = ProjectPaths::ProjectName();
    }
    return info;
}

bool ProjectManager::CreateProject(const std::string &name, std::string *outErrorMessage) {
    LogScope scope;
    const auto fail = [outErrorMessage](const std::string &message) {
        if (outErrorMessage) *outErrorMessage = message;
        Log("Failed to create project: " + message, LogSeverity::Error);
        return false;
    };

    if (!IsValidProjectName(name, outErrorMessage)) {
        Log("Failed to create project: invalid name '" + name + "'", LogSeverity::Error);
        return false;
    }

    std::error_code ec;
    const std::string projectRoot = ProjectPaths::ProjectsRoot() + "/" + name;
    const std::filesystem::path projectRootPath = Utf8StringToPath(projectRoot);
    if (std::filesystem::exists(projectRootPath, ec)) {
        return fail("A project named '" + name + "' already exists.");
    }

    const std::filesystem::path templateRoot = Utf8StringToPath(ProjectPaths::AssetsTemplateRoot());
    if (!std::filesystem::is_directory(templateRoot, ec)) {
        return fail("Assets template folder not found: " + ProjectPaths::AssetsTemplateRoot());
    }

    //--------- テンプレートをコピーしてAssetsフォルダを作る ---------//
    const std::filesystem::path assetsRoot = projectRootPath / ProjectPaths::kAssetsFolderName;
    std::filesystem::create_directories(assetsRoot, ec);
    if (ec) return fail("Could not create project folder: " + ec.message());

    std::filesystem::copy(templateRoot, assetsRoot,
        std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        // 中途半端に作られたフォルダが残ると次回の作成も失敗するため片付ける
        std::error_code removeError;
        std::filesystem::remove_all(projectRootPath, removeError);
        return fail("Could not copy the assets template: " + ec.message());
    }

    //--------- プロジェクト定義ファイルを書き出す ---------//
    ProjectInfo info;
    info.name = name;
    info.rootPath = ProjectPaths::NormalizeSeparators(projectRoot);
    info.formatVersion = kProjectFormatVersion;
    if (!WriteProjectFile(info)) {
        std::error_code removeError;
        std::filesystem::remove_all(projectRootPath, removeError);
        return fail("Could not write " + std::string(ProjectPaths::kProjectFileName) + ".");
    }

    Log("Created project: " + info.rootPath, LogSeverity::Info);
    return true;
}

void ProjectManager::SetStartupProject(const std::string &name) {
    UserSettings::SetString(UserSettings::kLastOpenedProjectKey, name);
}

std::string ProjectManager::GetStartupProject() {
    return UserSettings::GetString(UserSettings::kLastOpenedProjectKey, "");
}

bool ProjectManager::RequestRestartWithProject(const std::string &name) {
    LogScope scope;

    ProjectInfo info;
    if (!ReadProjectFile(ProjectPaths::ProjectsRoot() + "/" + name, info)) {
        Log("Cannot restart: project not found: " + name, LogSeverity::Error);
        return false;
    }

    // 次回以降ランチャーを介さずに起動された場合もこのプロジェクトが開くようにしておく
    SetStartupProject(name);
    sPendingRestartProjectName_ = name;
    Log("Restart requested for project: " + name, LogSeverity::Info);
    return true;
}

bool ProjectManager::StartEditorProcess(const std::string &projectName, std::string *outErrorMessage) {
    LogScope scope;
    const auto fail = [outErrorMessage](const std::string &message) {
        if (outErrorMessage) *outErrorMessage = message;
        Log("Failed to launch the editor: " + message, LogSeverity::Error);
        return false;
    };

    const std::string executableDirectory = ProjectPaths::ExecutableDirectory();
    const std::wstring executablePath = ConvertString(executableDirectory + "/" + kExecutableFileName);

    std::error_code ec;
    if (!std::filesystem::is_regular_file(executablePath, ec)) {
        return fail(std::string(kExecutableFileName) + " was not found next to this executable.");
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
        return fail("CreateProcess failed.");
    }

    // 起動したプロセスの終了を待つ必要はないため、ハンドルは即座に閉じる
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    Log("Launched the editor with project: " + projectName, LogSeverity::Info);
    return true;
}

bool ProjectManager::LaunchPendingRestart(PasskeyForGameEngineMain) {
    if (sPendingRestartProjectName_.empty()) return false;

    const std::string projectName = sPendingRestartProjectName_;
    sPendingRestartProjectName_.clear();
    return StartEditorProcess(projectName, nullptr);
}

bool ProjectManager::LaunchEditor(const std::string &name, std::string *outErrorMessage) {
    ProjectInfo info;
    if (!ReadProjectFile(ProjectPaths::ProjectsRoot() + "/" + name, info)) {
        if (outErrorMessage) *outErrorMessage = "Project not found: " + name;
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
    Log("No project available. Creating the default project.", LogSeverity::Info);
    if (!CreateProject(kDefaultProjectName)) return false;

    ProjectPaths::SetActiveProject(Passkey<ProjectManager>{},
        ProjectPaths::ProjectsRoot() + "/" + kDefaultProjectName);
    SetStartupProject(kDefaultProjectName);
    return true;
}

} // namespace KashipanEngine
