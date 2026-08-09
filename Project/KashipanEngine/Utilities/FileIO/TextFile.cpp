#include "TextFile.h"
#include "Directory.h"
#include <fstream>

#include "Utilities/Conversion/ConvertString.h"

namespace KashipanEngine {

TextFileData LoadTextFile(const std::string &filePath) {
    TextFileData textFileData;
    textFileData.filePath = filePath;
    // std::ifstream(const std::string&)はWindows上で現在のANSIコードページを使ってファイルを開くため、
    // 非ASCII文字を含むパスが開けない。path版のコンストラクタへ渡すことで回避する
    std::ifstream file(Utf8StringToPath(filePath));
    if (!file.is_open()) {
        return textFileData;
    }
    std::string line;
    while (std::getline(file, line)) {
        textFileData.lines.push_back(line);
    }
    file.close();
    return textFileData;
}

void SaveTextFile(const TextFileData &textFileData) {
    // 保存先フォルダが存在しない場合は作成する
    EnsureParentDirectoryExists(textFileData.filePath);
    std::ofstream file(Utf8StringToPath(textFileData.filePath));
    if (!file.is_open()) {
        return;
    }
    for (const auto &line : textFileData.lines) {
        file << line << '\n';
    }
    file.close();
}

} // namespace KashipanEngine