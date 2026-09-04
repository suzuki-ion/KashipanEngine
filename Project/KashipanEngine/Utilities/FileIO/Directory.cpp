#include "Directory.h"
#include <filesystem>

#include "Debug/Logger.h"
#include "Utilities/Conversion/ConvertString.h"

namespace KashipanEngine {

namespace {

/// @brief ファイル名取得用
/// @details path::string()はWindows上でANSIコードページを使うため、非ASCII文字を含む
///          ファイル名が文字化けする。常にPathToUtf8Stringで変換すること
std::string GetFileName(const std::filesystem::path &filePath, bool isFullPath) {
    LogScope scope;
    if (isFullPath) {
        return PathToUtf8String(filePath);
    } else {
        return PathToUtf8String(filePath.filename());
    }
}

/// @brief 指定ディレクトリの情報を構築する（必要に応じて再帰）
DirectoryData BuildDirectoryData(const std::filesystem::path &directoryPath, bool isRecursive, bool isFullPath) {
    LogScope scope;
    DirectoryData data{};

    // ディレクトリ名（ルート等で空の場合はフルパスを設定）
    data.directoryName = PathToUtf8String(directoryPath.filename());
    if (data.directoryName.empty()) {
        data.directoryName = PathToUtf8String(directoryPath);
    }

    // ディレクトリが存在しない場合は空のまま返す
    if (!std::filesystem::exists(directoryPath) || !std::filesystem::is_directory(directoryPath)) {
        return data;
    }

    // 中身を走査
    for (const auto &entry : std::filesystem::directory_iterator(directoryPath)) {
        if (entry.is_regular_file()) {
            data.files.emplace_back(GetFileName(entry.path(), isFullPath));
        } else if (entry.is_directory()) {
            if (isRecursive) {
                data.subdirectories.emplace_back(BuildDirectoryData(entry.path(), isRecursive, isFullPath));
            }
        }
    }

    return data;
}

} // namespace

bool IsDirectoryExist(const std::string &directoryPath) {
    const std::filesystem::path dirPath = Utf8StringToPath(directoryPath);
    return std::filesystem::exists(dirPath) && std::filesystem::is_directory(dirPath);
}

bool RemoveFile(const std::string &filePath) {
    LogScope scope;
    std::error_code ec;
    const std::filesystem::path path = Utf8StringToPath(filePath);
    std::filesystem::remove(path, ec);
    return !ec;
}

bool CreateDirectories(const std::string &directoryPath) {
    LogScope scope;
    if (directoryPath.empty()) return true;
    std::error_code ec;
    const std::filesystem::path dirPath = Utf8StringToPath(directoryPath);
    if (std::filesystem::exists(dirPath, ec)) {
        return std::filesystem::is_directory(dirPath, ec);
    }
    std::filesystem::create_directories(dirPath, ec);
    return !ec;
}

bool EnsureParentDirectoryExists(const std::string &filePath) {
    LogScope scope;
    std::error_code ec;
    const std::filesystem::path parent = Utf8StringToPath(filePath).parent_path();
    if (parent.empty()) return true; // カレントディレクトリ直下
    if (std::filesystem::exists(parent, ec)) return true;
    std::filesystem::create_directories(parent, ec);
    return !ec;
}

DirectoryData GetDirectoryData(const std::string &directoryPath, bool isRecursive, bool isFullPath) {
    LogScope scope;
    const std::filesystem::path dirPath = Utf8StringToPath(directoryPath);
    return BuildDirectoryData(dirPath, isRecursive, isFullPath);
}

DirectoryData GetDirectoryDataByExtension(const DirectoryData &directoryData, const std::vector<std::string> &extensions) {
    LogScope scope;
    DirectoryData filteredData;
    filteredData.directoryName = directoryData.directoryName;
    // ファイルをフィルタリング
    for (const auto &file : directoryData.files) {
        const std::filesystem::path filePath = Utf8StringToPath(file);
        std::string fileExt = filePath.extension().string();
        for (const auto &ext : extensions) {
            if (fileExt == ext) {
                filteredData.files.push_back(file);
                break;
            }
        }
    }
    // サブディレクトリを再帰的にフィルタリング
    for (const auto &subdir : directoryData.subdirectories) {
        DirectoryData filteredSubdir = GetDirectoryDataByExtension(subdir, extensions);
        if (!filteredSubdir.files.empty() || !filteredSubdir.subdirectories.empty()) {
            filteredData.subdirectories.push_back(filteredSubdir);
        }
    }
    return filteredData;
}

DirectoryData GetDirectoryDataByExtension(const std::string &directoryPath, const std::vector<std::string> &extensions, bool isRecursive, bool isFullPath) {
    LogScope scope;
    DirectoryData dirData = GetDirectoryData(directoryPath, isRecursive, isFullPath);
    return GetDirectoryDataByExtension(dirData, extensions);
}

} // namespace KashipanEngine