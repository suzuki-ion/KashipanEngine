#include "INI.h"
#include <fstream>

namespace KashipanEngine {

INIData LoadINIFile(const std::string &filePath) {
    INIData iniData;
    iniData.filePath = filePath;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return iniData;
    }

    std::string currentSection;
    std::string line;
    while (std::getline(file, line)) {
        // 行の前後の空白を削除
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        // コメント行をスキップ
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }
        // セクションの検出
        if (line.front() == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.size() - 2);
            iniData.sections[currentSection] = {};
        }
        // キーと値のペアの検出
        else {
            size_t equalPos = line.find('=');
            if (equalPos != std::string::npos) {
                std::string key = line.substr(0, equalPos);
                std::string value = line.substr(equalPos + 1);
                // キーと値の前後の空白を削除
                key.erase(0, key.find_first_not_of(" \t\r\n"));
                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                value.erase(0, value.find_first_not_of(" \t\r\n"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);
                iniData.sections[currentSection][key] = value;
            }
        }
    }
    file.close();
    return iniData;
}

void SaveINIFile(const INIData &iniData) {
    std::ofstream file(iniData.filePath);
    if (!file.is_open()) {
        return;
    }
    for (const auto &[section, keyValues] : iniData.sections) {
        file << "[" << section << "]\n";
        for (const auto &[key, value] : keyValues) {
            file << key << "=" << value << "\n";
        }
        file << "\n";
    }
    file.close();
}

} // namespace KashipanEngine