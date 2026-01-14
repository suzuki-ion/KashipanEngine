#pragma once
#include <string>
#include <vector>

namespace KashipanEngine {

/// @brief テキストファイルデータ構造体
struct TextFileData final {
    std::string filePath;           ///< ファイルパス
    std::vector<std::string> lines; ///< テキストファイルの各行データ
};

/// @brief テキストファイルを読み込む
/// @param filePath ファイルパス
TextFileData LoadTextFile(const std::string &filePath);

/// @brief テキストファイルを書き込む
/// @param textFileData テキストファイルデータ
void SaveTextFile(const TextFileData &textFileData);

} // namespace KashipanEngine