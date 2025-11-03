#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "Debug/Logger/LogType.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

/// @brief ログ設定構造体
struct LogSettings {
    bool enableFileLogging = true;
    bool enableConsoleLogging = true;
    bool enableOutputNamespace = true;
    bool enableOutputClass = true;
    std::string nestedTabs = "    ";
    std::string outputDirectory = "Logs";
    std::string logFileFormat = "${BuildType} ${Year}-${Month}-${Day}_${Hour}-${Minute}-${Second}";
    std::string logMessageFormat = "[${Year}:${Month}:${Day} ${Hour}:${Minute}:${Second}] ${NestedTabs}[ namespace ${namespace} ][ class ${class} ][ function ${function} ]: [${Severity}] ${Message}";
    std::vector<std::string> namespaces;
    std::unordered_map<std::string, bool> logLevelsEnabled = {
        { "Debug", true },
        { "Info", true },
        { "Warning", true },
        { "Error", true },
        { "Critical", true },
    };
};

const LogSettings &LoadLogSettings(PasskeyForGameEngineMain, const std::string &logSettingsFilePath);
const LogSettings &GetLogSettings();

} // namespace KashipanEngine