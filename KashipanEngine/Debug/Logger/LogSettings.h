#pragma once
#include <vector>
#include <string>
#include "LogType.h"

namespace KashipanEngine {

/// @brief 出力ログ設定用構造体
struct LogSettings {
    LogSettings(LogDomain logDomain, const std::vector<LogCategory> &logCategories)
        : domain(logDomain), categories(logCategories) {
    }
    const LogDomain domain;
    const std::vector<LogCategory> categories;
    LogSeverity severity = LogSeverity::Info;
    LogContentType contentType = LogContentType::Message;
    std::string logText = "Default log message.";
};

} // namespace KashipanEngine