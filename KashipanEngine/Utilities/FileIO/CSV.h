#pragma once
#include <string>
#include <vector>

namespace KashipanEngine {

/// @brief CSVデータ用構造体
struct CSVData final {
    std::string filePath;                       // ファイルパス
    std::vector<std::string> headers;           // ヘッダー行
    std::vector<std::vector<std::string>> rows; // データ行
};

/// @brief CSVファイルの読み込み
/// @param filePath CSVファイルのパス
/// @param hasHeader ヘッダー行があるかどうか。デフォルトはtrue
CSVData LoadCSV(const std::string &filePath, bool hasHeader = true);

/// @brief CSVファイルの保存
/// @param filePath CSVファイルのパス
/// @param data 保存するCSVデータ
void SaveCSV(const std::string &filePath, const CSVData &data);

} // namespace KashipanEngine
