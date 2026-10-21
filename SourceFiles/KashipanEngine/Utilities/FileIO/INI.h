#pragma once
#include <string>
#include <unordered_map>

namespace KashipanEngine {

/// @brief iniファイルデータ構造体
struct INIData final {
    std::string filePath; ///< ファイルパス
    /// @brief セクション名をキー、キーと値のペアのマップを値とするマップ
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> sections;
};

/// @brief INIファイルを読み込む
/// @param filePath ファイルパス
INIData LoadINIFile(const std::string &filePath);

/// @brief INIファイルを保存する
/// @param iniData INIデータ
void SaveINIFile(const INIData &iniData);

} // namespace KashipanEngine