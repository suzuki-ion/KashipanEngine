#include "LogSettings.h"
#include "Utilities/FileIO/Json.h"

namespace KashipanEngine {
namespace {
LogSettings sLogSettings;
} // anonymous

const LogSettings &LoadLogSettings(PasskeyForGameEngineMain, const std::string &logSettingsFilePath) {
    Json json = LoadJson(logSettingsFilePath);
    if (json.empty()) {
        return sLogSettings;
    }

    sLogSettings.enableFileLogging = json.value("enableFileLogging", sLogSettings.enableFileLogging);
    sLogSettings.enableConsoleLogging = json.value("enableConsoleLogging", sLogSettings.enableConsoleLogging);
    sLogSettings.nestedTabs = json.value("nestedTabs", sLogSettings.nestedTabs);
    sLogSettings.outputDirectory = json.value("outputDirectory", sLogSettings.outputDirectory);
    sLogSettings.logFileFormat = json.value("logFileFormat", sLogSettings.logFileFormat);
    sLogSettings.logMessageFormat = json.value("logMessageFormat", sLogSettings.logMessageFormat);
    auto logLevelsJson = json.value("logLevelsEnabled", Json::object());
    sLogSettings.logLevelsEnabled["Debug"] = logLevelsJson.value("Debug", sLogSettings.logLevelsEnabled["Debug"]);
    sLogSettings.logLevelsEnabled["Info"] = logLevelsJson.value("Info", sLogSettings.logLevelsEnabled["Info"]);
    sLogSettings.logLevelsEnabled["Warning"] = logLevelsJson.value("Warning", sLogSettings.logLevelsEnabled["Warning"]);
    sLogSettings.logLevelsEnabled["Error"] = logLevelsJson.value("Error", sLogSettings.logLevelsEnabled["Error"]);
    sLogSettings.logLevelsEnabled["Critical"] = logLevelsJson.value("Critical", sLogSettings.logLevelsEnabled["Critical"]);
    
    return sLogSettings;
}

const LogSettings &GetLogSettings() {
    return sLogSettings;
}

} // namespace KashipanEngine