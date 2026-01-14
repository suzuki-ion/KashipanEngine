#include "LogSettings.h"
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {
namespace {
LogSettings sLogSettings;
} // anonymous

const LogSettings &LoadLogSettings(PasskeyForGameEngineMain, const std::string &logSettingsFilePath) {
    JSON json = LoadJSON(logSettingsFilePath);
    if (json.empty()) {
        return sLogSettings;
    }

    sLogSettings.enableFileLogging = json.value("enableFileLogging", sLogSettings.enableFileLogging);
    sLogSettings.enableConsoleLogging = json.value("enableConsoleLogging", sLogSettings.enableConsoleLogging);
    sLogSettings.enableOutputNamespace = json.value("enableOutputNamespace", sLogSettings.enableOutputNamespace);
    sLogSettings.enableOutputClass = json.value("enableOutputClass", sLogSettings.enableOutputClass);

    sLogSettings.nestedTabs = json.value("nestedTabs", sLogSettings.nestedTabs);
    sLogSettings.outputDirectory = json.value("outputDirectory", sLogSettings.outputDirectory);
    sLogSettings.logFileFormat = json.value("logFileFormat", sLogSettings.logFileFormat);
    sLogSettings.logMessageFormat = json.value("logMessageFormat", sLogSettings.logMessageFormat);
    
    sLogSettings.namespaces.clear();
    if (json.contains("namespaces") && json["namespaces"].is_array()) {
        for (const auto &ns : json["namespaces"]) {
            if (ns.is_string()) sLogSettings.namespaces.push_back(ns.get<std::string>());
        }
    }
    auto logLevelsJSON = json.value("logLevelsEnabled", JSON::object());
    sLogSettings.logLevelsEnabled["Debug"] = logLevelsJSON.value("Debug", sLogSettings.logLevelsEnabled["Debug"]);
    sLogSettings.logLevelsEnabled["Info"] = logLevelsJSON.value("Info", sLogSettings.logLevelsEnabled["Info"]);
    sLogSettings.logLevelsEnabled["Warning"] = logLevelsJSON.value("Warning", sLogSettings.logLevelsEnabled["Warning"]);
    sLogSettings.logLevelsEnabled["Error"] = logLevelsJSON.value("Error", sLogSettings.logLevelsEnabled["Error"]);
    sLogSettings.logLevelsEnabled["Critical"] = logLevelsJSON.value("Critical", sLogSettings.logLevelsEnabled["Critical"]);
    
    return sLogSettings;
}

const LogSettings &GetLogSettings() {
    return sLogSettings;
}

} // namespace KashipanEngine