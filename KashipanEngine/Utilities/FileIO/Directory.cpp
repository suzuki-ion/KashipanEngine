#include "Directory.h"
#include <filesystem>

namespace KashipanEngine {

namespace {

/// @brief ファイル名取得用
std::string GetFileName(const std::filesystem::path &filePath, bool isFullPath) {
    if (isFullPath) {
        return filePath.string();
    } else {
        return filePath.filename().string();
    }
}

/// @brief 指定ディレクトリの情報を構築する（必要に応じて再帰）
DirectoryData BuildDirectoryData(const std::filesystem::path &directoryPath, bool isRecursive, bool isFullPath) {
    DirectoryData data{};

    // ディレクトリ名（ルート等で空の場合はフルパスを設定）
    data.directoryName = directoryPath.filename().string();
    if (data.directoryName.empty()) {
        data.directoryName = directoryPath.string();
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

DirectoryData GetDirectoryData(const std::string &directoryPath, bool isRecursive, bool isFullPath) {
    std::filesystem::path dirPath(directoryPath);
    return BuildDirectoryData(dirPath, isRecursive, isFullPath);
}

DirectoryData GetDirectoryDataByExtension(const DirectoryData &directoryData, const std::string &extension) {
    DirectoryData filteredData;
    filteredData.directoryName = directoryData.directoryName;
    // ファイルをフィルタリング
    for (const auto &file : directoryData.files) {
        std::filesystem::path filePath(file);
        if (filePath.extension() == extension) {
            filteredData.files.push_back(file);
        }
    }
    // サブディレクトリを再帰的にフィルタリング
    for (const auto &subdir : directoryData.subdirectories) {
        DirectoryData filteredSubdir = GetDirectoryDataByExtension(subdir, extension);
        if (!filteredSubdir.files.empty() || !filteredSubdir.subdirectories.empty()) {
            filteredData.subdirectories.push_back(filteredSubdir);
        }
    }
    return filteredData;
}

std::vector<std::string> FilterFilesByExtension(const std::vector<std::string> &filePaths, const std::vector<std::string> &extensions) {
    std::vector<std::string> filteredFiles;
    for (const auto &filePath : filePaths) {
        std::filesystem::path pathObj(filePath);
        std::string fileExt = pathObj.extension().string();
        for (const auto &ext : extensions) {
            if (fileExt == ext) {
                filteredFiles.push_back(filePath);
                break;
            }
        }
    }
    return filteredFiles;
}

} // namespace KashipanEngine