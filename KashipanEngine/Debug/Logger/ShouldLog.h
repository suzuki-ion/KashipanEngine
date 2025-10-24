#pragma once

// ログレベルが有効かどうか
bool ShouldLog(LogSeverity severity) {
    const auto &levels = GetLogSettings().logLevelsEnabled;
    switch (severity) {
    case LogSeverity::Debug:    { auto it = levels.find("Debug");    return it == levels.end() ? true : it->second; }
    case LogSeverity::Info:     { auto it = levels.find("Info");     return it == levels.end() ? true : it->second; }
    case LogSeverity::Warning:  { auto it = levels.find("Warning");  return it == levels.end() ? true : it->second; }
    case LogSeverity::Error:    { auto it = levels.find("Error");    return it == levels.end() ? true : it->second; }
    case LogSeverity::Critical: { auto it = levels.find("Critical"); return it == levels.end() ? true : it->second; }
    default:
        return true;
    }
}
