#pragma once
#include <string>
#include <vector>

namespace KashipanEngine {

/// @brief ディレクトリデータ構造体
struct DirectoryData final {
    std::string directoryName;                  ///< ディレクトリ名
    std::vector<std::string> files;             ///< ファイルのリスト
    std::vector<DirectoryData> subdirectories;  ///< サブディレクトリのリスト
};

/// @brief ディレクトリが存在するかどうか確認
/// @param directoryPath ディレクトリパス
bool IsDirectoryExist(const std::string &directoryPath);

/// @brief ディレクトリ情報取得
/// @param directoryPath ディレクトリパス
/// @param isRecursive 再帰的にサブディレクトリ内も検索するかどうか
/// @param isFullPath ファイルをフルパスで取得するかどうか（false の場合はファイル名のみ）
/// @return 指定ディレクトリをルートとした DirectoryData
DirectoryData GetDirectoryData(const std::string &directoryPath, bool isRecursive = false, bool isFullPath = true);

/// @brief ディレクトリ情報から指定の拡張子のファイル一覧を取得
/// @param directoryData ディレクトリデータ
/// @param extensions 拡張子（例: { ".txt", ".png" }）
/// @return 指定拡張子のファイル一覧
DirectoryData GetDirectoryDataByExtension(const DirectoryData &directoryData, const std::vector<std::string> &extensions);

/// @brief ディレクトリ情報取得（拡張子指定版）
/// @param directoryPath ディレクトリパス
/// @param extensions 拡張子（例: { ".txt", ".png" }）
/// @param isRecursive 再帰的にサブディレクトリ内も検索するかどうか
/// @param isFullPath ファイルをフルパスで取得するかどうか（false の場合はファイル名のみ）
/// @return 指定拡張子のファイル一覧
DirectoryData GetDirectoryDataByExtension(const std::string &directoryPath, const std::vector<std::string> &extensions, bool isRecursive = false, bool isFullPath = true);

} // namespace KashipanEngine
