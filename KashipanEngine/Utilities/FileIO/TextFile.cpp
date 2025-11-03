#include "TextFile.h"
#include <fstream>

namespace KashipanEngine {

TextFileData LoadTextFile(const std::string &filePath) {
    TextFileData textFileData;
    textFileData.filePath = filePath;
    std::ifstream file(filePath);
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
    std::ofstream file(textFileData.filePath);
    if (!file.is_open()) {
        return;
    }
    for (const auto &line : textFileData.lines) {
        file << line << '\n';
    }
    file.close();
}

} // namespace KashipanEngine