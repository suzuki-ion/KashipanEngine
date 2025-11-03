#pragma once
#include <source_location>
#include "Debug/Logger/LogType.h"
#include "Utilities/Translation.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

/// @brief ロガー初期化
void InitializeLogger(PasskeyForGameEngineMain);
/// @brief ロガー終了
void ShutdownLogger(PasskeyForGameEngineMain);
/// @brief ロガー強制終了（クラッシュハンドラ用）
void ForceShutdownLogger(PasskeyForCrashHandler);

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
    }
    /// @brief デストラクタ
    ~LogScope() {
        PopPrefix();
    }
private:
    /// @brief ログにプレフィックスを挿入
    void PushPrefix(const std::source_location &location = std::source_location::current());
    /// @brief ログからプレフィックスを削除
    void PopPrefix();
};

} // namespace KashipanEngine