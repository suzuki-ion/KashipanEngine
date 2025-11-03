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

/// @brief ディレクトリ情報取得
/// @param directoryPath ディレクトリパス
/// @param isRecursive 再帰的にサブディレクトリ内も検索するかどうか
/// @param isFullPath ファイルをフルパスで取得するかどうか（false の場合はファイル名のみ）
/// @return 指定ディレクトリをルートとした DirectoryData
DirectoryData GetDirectoryData(const std::string &directoryPath, bool isRecursive = false, bool isFullPath = true);

/// @brief ディレクトリ情報から指定の拡張子のファイル一覧を取得
/// @param directoryData ディレクトリデータ
/// @param extension 拡張子（例: ".txt"）
/// @return 指定拡張子のファイル一覧
DirectoryData GetDirectoryDataByExtension(const DirectoryData &directoryData, const std::string &extension);

} // namespace KashipanEngine
