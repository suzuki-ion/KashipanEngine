#include "Logger.h"
#include "LogSettings.h"
#include "Utilities/TimeUtils.h"
#include "Utilities/SourceLocation.h"
#include "Utilities/TemplateLiteral.h"
#include <Windows.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <string_view>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cctype>

namespace KashipanEngine {
namespace {
bool sLoggerInitialized = false;
std::ofstream sLogFile;
const std::string kBuildTypeString =
#ifdef DEBUG_BUILD
"[Debug]";
#elif defined(DEVELOP_BUILD)
"[Develop]";
#elif defined(RELEASE_BUILD)
"[Release]";
#else
"[Unknown]";
#endif
int sTabCount = -1;

// スコープフレーム
struct ScopeFrame {
    std::vector<std::string> scopes; // 外側→内側（namespace / class を区別しない）
    std::string functionName;
    int depth = -1;
    bool enteredFlushed = false;
    bool hadOutput = false;
};
std::vector<ScopeFrame> sScopeFrames;

// ラベル
const std::unordered_map<LogSeverity, std::string> kLogSeverityLabel = {
    { LogSeverity::Debug, "DEBUG" },
    { LogSeverity::Info, "INFO" },
    { LogSeverity::Warning, "WARNING" },
    { LogSeverity::Error, "ERROR" },
    { LogSeverity::Critical, "CRITICAL" },
};

// 先に宣言（後のヘッダで参照するため)
const ScopeFrame* GetTopScopeFrame();

//--------- 関数別ヘッダの取り込み（Logger/*）---------//
#include "Logger/Trim.h"
#include "Logger/Qualifiers.h"
#include "Logger/TemplateParse.h"
#include "Logger/UserType.h"
#include "Logger/NamespaceClass.h"
#include "Logger/ScopeInfo.h"
#include "Logger/Repeat.h"
#include "Logger/GetTabs.h"
#include "Logger/StringFormat.h"
#include "Logger/TimeTokens.h"
#include "Logger/BuildLogLine.h"
#include "Logger/WriteLog.h"
#include "Logger/FlushPending.h"
#include "Logger/BuildLogFileName.h"
#include "Logger/ShouldLog.h"

// 実体
const ScopeFrame* GetTopScopeFrame() {
    if (sScopeFrames.empty()) return nullptr;
    return &sScopeFrames.back();
}

} // namespace

void InitializeLogger(PasskeyForGameEngineMain) {
    if (sLoggerInitialized) return;
    const auto &cfg = GetLogSettings();

    if (cfg.enableFileLogging) {
        const std::string currentDir = std::filesystem::current_path().string();
        const std::string logDir = currentDir + "/" + cfg.outputDirectory;
        std::filesystem::create_directories(logDir);
        const std::string logFilePath = logDir + "/" + BuildLogFileName();
        sLogFile.open(logFilePath, std::ios::out);
        if (!sLogFile) {
            assert(false && "ログファイルのオープンに失敗しました。");
        }
        if (ShouldLog(LogSeverity::Info)) {
            WriteLog(BuildLogLine(nullptr, LogSeverity::Info, std::string("Log File: ") + logFilePath));
        }
    }
    if (ShouldLog(LogSeverity::Info)) {
        WriteLog(BuildLogLine(nullptr, LogSeverity::Info, "----- Log Start -----"));
    }

    sLoggerInitialized = true;
}

void ShutdownLogger(PasskeyForGameEngineMain) {
    if (!sLoggerInitialized) return;
    if (ShouldLog(LogSeverity::Info)) {
        WriteLog(BuildLogLine(nullptr, LogSeverity::Info, "----- Log End -----"));
    }
    if (sLogFile.is_open()) {
        sLogFile.close();
    }
    sLoggerInitialized = false;
}

void ForceShutdownLogger(PasskeyForCrashHandler) {
    if (!sLoggerInitialized) return;
    if (sLogFile.is_open()) {
        sLogFile.close();
    }
    sLoggerInitialized = false;
}

void Log(const std::string &logText, LogSeverity severity) {
    // レベルが無効なら何もしない
    if (!ShouldLog(severity)) return;

    if (!sScopeFrames.empty()) {
        FlushPendingScopeEnters();
        for (auto &frame : sScopeFrames) frame.hadOutput = true;
    }

    WriteLog(BuildLogLine(GetTopScopeFrame(), severity, logText));
}

void LogSeparator() {
    Log("--------------------------------------------------", LogSeverity::Info);
}

void LogScope::PushPrefix(const std::source_location &location) {
    ++sTabCount;

    // Utilities の SourceLocation を使用してスコープ情報を構築
    SourceLocationInfo info = MakeSourceLocationInfo(location);

    ScopeFrame frame{};
    frame.scopes       = info.signature.scopes;
    frame.functionName = info.signature.functionName;
    frame.depth = sTabCount;
    sScopeFrames.push_back(std::move(frame));
}

void LogScope::PopPrefix() {
    ScopeFrame frame = sScopeFrames.back();
    if (frame.hadOutput && frame.enteredFlushed) {
        if (ShouldLog(LogSeverity::Debug)) {
            WriteLog(BuildLogLine(&frame, LogSeverity::Debug, "Exiting scope."));
        }
    }
    sScopeFrames.pop_back();
    --sTabCount;
}

} // namespace KashipanEngine
