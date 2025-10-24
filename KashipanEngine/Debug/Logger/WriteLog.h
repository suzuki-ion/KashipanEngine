#pragma once

// ログ出力
void WriteLog(const std::string &formattedLine) {
    const auto &cfg = GetLogSettings();
    if (cfg.enableConsoleLogging) {
        OutputDebugStringA(formattedLine.c_str());
    }
    if (cfg.enableFileLogging && sLogFile.is_open()) {
        sLogFile << formattedLine;
    }
}
