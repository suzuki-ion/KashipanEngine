#pragma once
#include <vector>
#include <string>
#include <source_location>
#include "Debug/Logger/LogSettings.h"

namespace KashipanEngine {

/// @brief ログ出力
/// @param logText 出力するログテキスト
/// @param severity ログのレベル。指定が無い場合は Info になる。
void Log(const std::string &logText, LogSeverity severity = LogSeverity::Info);

/// @brief 分離線ログ出力
void LogSeparator();

/// @brief ログスコープクラス
class LogScope {
public:
    /// @brief コンストラクタ
    LogScope(const std::source_location &location = std::source_location::current()) {
        PushPrefix(location);
        Log("Entering scope.", LogSeverity::Debug);
    }
    /// @brief デストラクタ
    ~LogScope() {
        Log("Exiting scope.", LogSeverity::Debug);
        PopPrefix();
    }
private:
    /// @brief ログにプレフィックスを挿入
    void PushPrefix(const std::source_location &location = std::source_location::current());
    /// @brief ログからプレフィックスを削除
    void PopPrefix();
};

} // namespace KashipanEngine