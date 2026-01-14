#pragma once
#include <source_location>
#include "Utilities/Translation.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

/// @brief ログの大分類
enum class LogDomain {
    GameEngine, // エンジン関連
    Application // ゲーム関連
};

/// @brief ログの情報の種類
enum class LogSeverity {
    Debug,      // デバッグ
    Info,       // 情報
    Warning,    // 警告
    Error,      // エラー
    Critical    // 致命的エラー
};

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