#pragma once
#include <source_location>
#include <string>
#include <vector>
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

/// @brief 1行分のログ（スプラッシュ画面など、外部へログを転送する用途向け）
struct LogLine {
    std::string text;
    LogSeverity severity = LogSeverity::Info;
};

class SplashScreen;

/// @brief スプラッシュ画面向けのログ収集を開始する
/// @details 呼び出し以降に出力されたログを内部バッファへ溜め始める。
///          通常のシンク（コンソール/ファイル/ImGui）には影響しない。
void BeginSplashLogCapture(Passkey<SplashScreen>);
/// @brief スプラッシュ画面向けのログ収集を終了し、溜めていたログを破棄する
void EndSplashLogCapture(Passkey<SplashScreen>);
/// @brief 前回の呼び出し以降に追加されたログ行を取り出す（取り出した分はバッファから消える）
std::vector<LogLine> FetchNewSplashLogLines(Passkey<SplashScreen>);

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

#ifdef USE_IMGUI
class SceneEditor;
void ShowImGuiLoggerWindow(Passkey<SceneEditor>);
#endif

} // namespace KashipanEngine