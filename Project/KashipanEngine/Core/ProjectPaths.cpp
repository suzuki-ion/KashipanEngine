#include "ProjectPaths.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>

#include <shellapi.h>
#pragma comment(lib, "Shell32.lib")

#include "Debug/Logger.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/Directory.h"
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {
namespace {

/// @brief 全プロジェクト共有のユーザー設定（新形式：UserSettings/Settings.json、エンジンルート直下）
/// @details 通常は UserSettings クラスが読み書きするファイル。ここでは前回開いたプロジェクト名
///          だけが必要だが、UserSettings 自体が ProjectPaths::EngineRoot() に依存するため直接読む。
constexpr const char *kUserSettingsFileName = "UserSettings/Settings.json";

/// @brief 旧形式（単一ファイル）のユーザー設定ファイル名。UserSettings::EnsureLoaded()による
///        新形式への移行がまだ行われていないタイミングでも前回開いたプロジェクトを読めるよう、
///        新形式が見つからない場合のフォールバックとして使う
constexpr const char *kLegacyUserSettingsFileName = "UserSettings.json";

/// @brief UserSettings.json 内で前回開いたプロジェクト名を保持するキー
constexpr const char *kLastOpenedProjectKey = "project.lastOpened";

/// @brief exeのフルパスを取得する
std::filesystem::path GetExecutablePath() {
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) return {};
        // バッファ長ぴったりの場合は切り詰められた可能性があるため倍にして再取得する
        if (length < buffer.size()) {
            buffer.resize(length);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::filesystem::path(buffer);
}

/// @brief EngineRoot.txt を読み、記載されたフォルダが存在すればそのパスを返す
std::string ReadEngineRootMarker(const std::filesystem::path &markerFilePath) {
    std::ifstream file(markerFilePath);
    if (!file) return {};

    std::string line;
    if (!std::getline(file, line)) return {};

    // ビルド後イベントのechoは末尾に空白や改行を残すことがあるため取り除く
    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    line.erase(line.begin(), std::find_if(line.begin(), line.end(), notSpace));
    line.erase(std::find_if(line.rbegin(), line.rend(), notSpace).base(), line.end());
    if (line.empty()) return {};

    std::error_code ec;
    const std::filesystem::path root = Utf8StringToPath(line);
    if (!std::filesystem::is_directory(root, ec)) return {};
    return PathToUtf8String(std::filesystem::weakly_canonical(root, ec));
}

} // namespace

std::string ProjectPaths::NormalizeSeparators(std::string path) {
    LogScope scope;
    std::replace(path.begin(), path.end(), '\\', '/');
    while (path.size() > 1 && path.back() == '/') path.pop_back();
    return path;
}

void ProjectPaths::Initialize(PasskeyForGameEngineMain) {
    LogScope scope;
    if (sIsInitialized) return;
    sIsInitialized = true;

    ResolveEngineRoot();

    // exeと同じフォルダに Assets が直置きされている＝配布形態。
    // この場合はプロジェクトの選択を行わず、exeのフォルダ自体をプロジェクトルートとして扱う
    std::error_code ec;
    const bool hasSideBySideAssets =
        std::filesystem::is_directory(Utf8StringToPath(sExecutableDirectory + "/" + kAssetsFolderName), ec);
    const bool hasEngineRootMarker =
        std::filesystem::is_regular_file(Utf8StringToPath(sExecutableDirectory + "/" + kEngineRootMarkerFileName), ec);

    if (hasSideBySideAssets && !hasEngineRootMarker) {
        sIsStandalone = true;
        sProjectRoot = sExecutableDirectory;
        sAssetsRoot = sProjectRoot + "/" + kAssetsFolderName;
        sProjectName = PathToUtf8String(Utf8StringToPath(sProjectRoot).filename());
        Log(Translation("engine.project.standalone") + sProjectRoot, LogSeverity::Info);
        return;
    }

    sIsStandalone = false;
    ResolveActiveProject();
}

void ProjectPaths::InitializeEngineRootOnly(PasskeyForWinMain) {
    LogScope scope;
    ResolveEngineRoot();
    // プロジェクトを開かないため、配布形態かどうかの判定も行わない
    sIsStandalone = false;
}

void ProjectPaths::ResolveEngineRoot() {
    LogScope scope;
    // Initialize() より先に InitializeEngineRootOnly() が呼ばれることがあるため、
    // 二重に解決しないようここでガードする（Initialize() 側のガードとは別）
    if (sIsEngineRootResolved) return;
    sIsEngineRootResolved = true;

    const std::filesystem::path exePath = GetExecutablePath();
    sExecutableDirectory = NormalizeSeparators(PathToUtf8String(exePath.parent_path()));

    // 開発時はexeがGenerated/Outputs配下に出力されるため、ビルド後イベントが書き出した
    // EngineRoot.txt を辿ってソースツリー側のフォルダをエンジンルートとして扱う
    const std::string markerRoot =
        ReadEngineRootMarker(Utf8StringToPath(sExecutableDirectory + "/" + kEngineRootMarkerFileName));
    sEngineRoot = markerRoot.empty() ? sExecutableDirectory : NormalizeSeparators(markerRoot);
}

std::string ProjectPaths::GetProjectArgument() {
    LogScope scope;
    int argumentCount = 0;
    LPWSTR *arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (!arguments) return {};

    std::string value;
    const std::wstring optionName = ConvertString(std::string(kProjectArgumentName));
    for (int i = 1; i < argumentCount; ++i) {
        if (optionName != arguments[i]) continue;
        // "--project" の次の引数が値。末尾に置かれていた場合は値なしとして扱う
        if (i + 1 < argumentCount) value = ConvertString(std::wstring(arguments[i + 1]));
        break;
    }

    LocalFree(arguments);
    return value;
}

void ProjectPaths::ResolveActiveProject() {
    LogScope scope;
    std::error_code ec;
    const std::filesystem::path projectsRoot = Utf8StringToPath(ProjectsRoot());

    const auto tryOpen = [&](const std::filesystem::path &candidate) {
        if (!std::filesystem::is_regular_file(candidate / Utf8StringToPath(kProjectFileName), ec)) return false;
        sProjectRoot = NormalizeSeparators(PathToUtf8String(candidate));
        sAssetsRoot = sProjectRoot + "/" + kAssetsFolderName;
        sProjectName = PathToUtf8String(candidate.filename());
        return true;
    };

    //--------- コマンドラインでの指定（ランチャー・プロジェクト切り替えによる再起動） ---------//
    const std::string projectArgument = GetProjectArgument();
    if (!projectArgument.empty()) {
        // プロジェクト名と絶対パスのどちらでも受け付ける
        const std::filesystem::path specified = Utf8StringToPath(projectArgument);
        const std::filesystem::path candidate =
            specified.is_absolute() ? specified : projectsRoot / specified;
        if (tryOpen(candidate)) {
            Log(Translation("engine.project.opened.commandline") + sProjectRoot, LogSeverity::Info);
            return;
        }
        Log(Translation("engine.project.opened.commandline.notfound") + projectArgument, LogSeverity::Warning);
    }

    //--------- 前回開いたプロジェクト ---------//
    JSON userSettings = LoadJSON(InEngineRoot(kUserSettingsFileName));
    if (!userSettings.is_object()) {
        // 新形式への移行がまだ行われていない（UserSettingsが一度も使われていない）場合のフォールバック
        userSettings = LoadJSON(InEngineRoot(kLegacyUserSettingsFileName));
    }
    if (userSettings.is_object()) {
        const auto it = userSettings.find(kLastOpenedProjectKey);
        if (it != userSettings.end() && it->is_string()) {
            const std::string lastOpened = it->get<std::string>();
            if (!lastOpened.empty() && tryOpen(projectsRoot / Utf8StringToPath(lastOpened))) {
                Log(Translation("engine.project.opened.lastused") + sProjectRoot, LogSeverity::Info);
                return;
            }
        }
    }

    //--------- Projects配下に存在する最初のプロジェクト ---------//
    if (std::filesystem::is_directory(projectsRoot, ec)) {
        for (const auto &entry : std::filesystem::directory_iterator(
                 projectsRoot, std::filesystem::directory_options::skip_permission_denied, ec)) {
            if (!entry.is_directory(ec)) continue;
            if (tryOpen(entry.path())) {
                Log(Translation("engine.project.opened") + sProjectRoot, LogSeverity::Info);
                return;
            }
        }
    }

    // 1つも見つからなかった場合はProjectManagerが新規作成するため、ここでは空のままにする
    Log(Translation("engine.project.notfound.any") + ProjectsRoot(), LogSeverity::Warning);
}

const std::string &ProjectPaths::ExecutableDirectory() {
    LogScope scope;
    return sExecutableDirectory;
}
const std::string &ProjectPaths::EngineRoot() {
    LogScope scope;
    return sEngineRoot;
}
const std::string &ProjectPaths::ProjectRoot() {
    LogScope scope;
    return sProjectRoot;
}
const std::string &ProjectPaths::AssetsRoot() {
    LogScope scope;
    return sAssetsRoot;
}
const std::string &ProjectPaths::ProjectName() {
    LogScope scope;
    return sProjectName;
}
bool ProjectPaths::IsStandalone() {
    LogScope scope;
    return sIsStandalone;
}

bool ProjectPaths::HasActiveProject() {
    LogScope scope;
    if (sProjectRoot.empty()) return false;
    if (sIsStandalone) return true;
    std::error_code ec;
    return std::filesystem::is_regular_file(Utf8StringToPath(sProjectRoot + "/" + kProjectFileName), ec);
}

std::string ProjectPaths::ToPhysical(const std::string &logicalPath) {
    LogScope scope;
    if (logicalPath.empty()) return logicalPath;

    const std::string normalized = NormalizeSeparators(logicalPath);
    // 既に物理パスへ変換済みの値を渡されても壊れないようにする
    if (Utf8StringToPath(normalized).is_absolute()) return normalized;
    if (sProjectRoot.empty()) return normalized;

    // "./Assets/..." のような表記も受け付ける
    std::string relative = normalized;
    if (relative.rfind("./", 0) == 0) relative.erase(0, 2);
    return sProjectRoot + "/" + relative;
}

std::string ProjectPaths::ToLogical(const std::string &physicalPath) {
    LogScope scope;
    if (physicalPath.empty()) return physicalPath;

    const std::string normalized = NormalizeSeparators(physicalPath);
    if (sProjectRoot.empty()) return normalized;

    std::error_code ec;
    const auto relative = std::filesystem::relative(
        Utf8StringToPath(normalized), Utf8StringToPath(sProjectRoot), ec);
    if (ec) return normalized;

    const std::string relativeString = NormalizeSeparators(PathToUtf8String(relative));
    // プロジェクトの外を指している場合は論理パスへ変換できないため、そのまま返す
    if (relativeString.rfind("..", 0) == 0) return normalized;

    return relativeString;
}

std::string ProjectPaths::InProjectRoot(const std::string &relativePath) {
    LogScope scope;
    if (sProjectRoot.empty()) return NormalizeSeparators(relativePath);
    return sProjectRoot + "/" + NormalizeSeparators(relativePath);
}

std::string ProjectPaths::InEngineRoot(const std::string &relativePath) {
    LogScope scope;
    if (sEngineRoot.empty()) return NormalizeSeparators(relativePath);
    return sEngineRoot + "/" + NormalizeSeparators(relativePath);
}

std::string ProjectPaths::AssetsTemplateRoot() {
    LogScope scope;
    return InEngineRoot(kAssetsTemplateFolderName);
}
std::string ProjectPaths::ProjectsRoot() {
    LogScope scope;
    return InEngineRoot(kProjectsFolderName);
}

std::vector<std::string> ProjectPaths::ListAssetFiles(const std::vector<std::string> &extensions) {
    LogScope scope;
    std::vector<std::string> files;
    if (sAssetsRoot.empty()) return files;

    const DirectoryData root = extensions.empty()
        ? GetDirectoryData(sAssetsRoot, true, true)
        : GetDirectoryDataByExtension(sAssetsRoot, extensions, true, true);

    // DirectoryDataは階層構造のままファイルを保持するため、一覧として扱えるよう平坦化する
    std::function<void(const DirectoryData &)> flatten = [&](const DirectoryData &directory) {
        for (const auto &file : directory.files) files.push_back(ToLogical(file));
        for (const auto &subdirectory : directory.subdirectories) flatten(subdirectory);
    };
    flatten(root);

    std::sort(files.begin(), files.end());
    return files;
}

void ProjectPaths::SetActiveProject(Passkey<ProjectManager>, const std::string &projectRootPath) {
    LogScope scope;
    sProjectRoot = NormalizeSeparators(projectRootPath);
    sAssetsRoot = sProjectRoot + "/" + kAssetsFolderName;
    sProjectName = PathToUtf8String(Utf8StringToPath(sProjectRoot).filename());
    Log(Translation("engine.project.active.changed") + sProjectRoot, LogSeverity::Info);
}

} // namespace KashipanEngine
