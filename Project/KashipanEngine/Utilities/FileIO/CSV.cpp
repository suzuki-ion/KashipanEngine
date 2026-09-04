#include "CSV.h"
#include "Directory.h"
#include <fstream>

#include "Debug/Logger.h"
#include "Utilities/Conversion/ConvertString.h"

namespace KashipanEngine {

CSVData LoadCSV(const std::string &filePath, bool hasHeader) {
    LogScope scope;
    CSVData data;
    // std::ifstream(const std::string&)はWindows上で現在のANSIコードページを使ってファイルを開くため、
    // 非ASCII文字を含むパスが開けない。path版のコンストラクタへ渡すことで回避する
    std::ifstream file(Utf8StringToPath(filePath));
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open CSV file: " + filePath);
    }
    std::string line;
    // ヘッダー行の読み込み
    if (hasHeader && std::getline(file, line)) {
        size_t start = 0;
        size_t end = line.find(',');
        while (end != std::string::npos) {
            data.headers.push_back(line.substr(start, end - start));
            start = end + 1;
            end = line.find(',', start);
        }
        data.headers.push_back(line.substr(start));
    }
    // データ行の読み込み
    while (std::getline(file, line)) {
        std::vector<std::string> row;
        size_t start = 0;
        size_t end = line.find(',');
        while (end != std::string::npos) {
            row.push_back(line.substr(start, end - start));
            start = end + 1;
            end = line.find(',', start);
        }
        row.push_back(line.substr(start));
        data.rows.push_back(row);
    }
    file.close();
    return data;
}

void SaveCSV(const std::string &filePath, const CSVData &data) {
    LogScope scope;
    // 保存先フォルダが存在しない場合は作成する
    EnsureParentDirectoryExists(filePath);
    std::ofstream file(Utf8StringToPath(filePath));
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open CSV file for writing: " + filePath);
    }
    // ヘッダー行の書き込み
    if (!data.headers.empty()) {
        for (size_t i = 0; i < data.headers.size(); ++i) {
            file << data.headers[i];
            if (i < data.headers.size() - 1) {
                file << ",";
            }
        }
        file << "\n";
    }
    // データ行の書き込み
    for (const auto &row : data.rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            file << row[i];
            if (i < row.size() - 1) {
                file << ",";
            }
        }
        file << "\n";
    }
    file.close();
}

} // namespace KashipanEngine