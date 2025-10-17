#include "Logger.h"
#include "Utilities/TimeUtils.h"
#include <Windows.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace KashipanEngine::Log {

namespace {
/// @brief ログファイルストリーム
std::ofstream sLogFile;
/// @brief ビルドの種類の文字列
const std::string kBuildTypeString =
#ifdef DEBUG_BUILD
"[Debug]";
#elif defined(RELEASE_BUILD)
"[Release]";
#else
"[Unknown]";
#endif
/// @brief ログのタブ数
int sTabCount = 0;
/// @brief タブの空白の数
const int kTabSpaceCount = 4;

/// @brief タブ文字列を取得
std::string GetTabString() {
    return std::string(sTabCount * kTabSpaceCount, ' ');
}
/// @brief ログを書き込む
void WriteLog(const std::string &logMessage) {
    // コンソールに出力
    OutputDebugStringA((logMessage + "\n").c_str());
    // ファイルに出力
    if (sLogFile.is_open()) {
        sLogFile << logMessage << std::endl;
    }
}

/// @brief ログ大分類文字列マップ
const std::unordered_map<LogDomain, std::string> kLogDomainToString = {
    { LogDomain::Engine, "[ENGINE]" },
    { LogDomain::Game, "[GAME]" },
};
/// @brief ログ中分類文字列マップ
const std::unordered_map<LogCategory, std::string> kLogCategoryToString = {
    { LogCategory::Core, "[CORE]" },
    { LogCategory::Graphics, "[GRAPHICS]" },
    { LogCategory::Texture, "[TEXTURE]" },
    { LogCategory::Audio, "[AUDIO]" },
    { LogCategory::Input, "[INPUT]" },
    { LogCategory::Resource, "[RESOURCE]" },
    { LogCategory::Scripting, "[SCRIPTING]" },
    { LogCategory::Physics, "[PHYSICS]" },
    { LogCategory::Gameplay, "[GAMEPLAY]" },
    { LogCategory::AI, "[AI]" },
    { LogCategory::UI, "[UI]" },
    { LogCategory::Animation, "[ANIMATION]" },
    { LogCategory::Scene, "[SCENE]" },
    { LogCategory::Debug, "[DEBUG]" },
    { LogCategory::Performance, "[PERFORMANCE]" },
    { LogCategory::Object, "[OBJECT]" },
    { LogCategory::Other, "[OTHER]" },
};
/// @brief ログ情報文字列マップ
const std::unordered_map<LogSeverity, std::string> kLogSeverityToString = {
    { LogSeverity::Info, "[INFO]" },
    { LogSeverity::Warning, "[WARNING]" },
    { LogSeverity::Error, "[ERROR]" },
};
/// @brief ログ内容文字列マップ
const std::unordered_map<LogContentType, std::string> kLogContentTypeToString = {
    { LogContentType::Message, "[MESSAGE]" },
    { LogContentType::Assertion, "[ASSERTION]" },
    { LogContentType::Exception, "[EXCEPTION]" },
    { LogContentType::Performance, "[PERFORMANCE]" },
    { LogContentType::Allocation, "[ALLOCATION]" },
    { LogContentType::StateChange, "[STATE_CHANGE]" },
    { LogContentType::Event, "[EVENT]" },
    { LogContentType::Other, "[OTHER]" },
};

/// @brief ファイルストリーム初期化用クラス
class FileStreamInitializer {
public:
    FileStreamInitializer() {
        const std::string currentDir = std::filesystem::current_path().string();
        const std::string logDir = currentDir + "/" + outputDir_;
        const std::string logFilePath = logDir + "/" + GetNowTimeString("%Y%m%d_%H%M%S") + ".log";
        std::filesystem::create_directories(logDir);
        sLogFile.open(logFilePath, std::ios::out);
        if (!sLogFile) {
            assert(false && "ログファイルのオープンに失敗しました。");
            return;
        }
    }
    ~FileStreamInitializer() {
        if (sLogFile.is_open()) {
            WriteLog("----- Log End -----");
            sLogFile.close();
        }
    }
private:
    const std::string outputDir_ = "Logs/";
} sFileStreamInitializer;

} // namespace

void OutputMessage(const LogSettings &settings) {
    std::string logMessage;
    // タブ
    logMessage += GetTabString();
    // ビルドの種類
    logMessage += kBuildTypeString + " ";
    // ログ大分類
    auto domainIt = kLogDomainToString.find(settings.domain);
    if (domainIt != kLogDomainToString.end()) {
        logMessage += domainIt->second + "/";
    } else {
        logMessage += "[UNKNOWN_DOMAIN]/";
    }
    // ログ中分類
    for (const auto &category : settings.categories) {
        auto categoryIt = kLogCategoryToString.find(category);
        if (categoryIt != kLogCategoryToString.end()) {
            logMessage += categoryIt->second + "/";
        } else {
            logMessage += "[UNKNOWN_CATEGORY]/";
        }
    }
    // ログ情報
    auto severityIt = kLogSeverityToString.find(settings.severity);
    if (severityIt != kLogSeverityToString.end()) {
        logMessage += severityIt->second + "/";
    } else {
        logMessage += "[UNKNOWN_SEVERITY]/";
    }
    // ログ内容
    auto contentTypeIt = kLogContentTypeToString.find(settings.contentType);
    if (contentTypeIt != kLogContentTypeToString.end()) {
        logMessage += contentTypeIt->second + ": ";
    } else {
        logMessage += "[UNKNOWN_CONTENT_TYPE]: ";
    }
    // 実際のログメッセージを追加
    logMessage += settings.logText;
    // ファイルに出力
    WriteLog(logMessage);
}

void OutputTime() {
    std::string timeString = GetNowTimeString("%Y-%m-%d %H:%M:%S");
    std::string logMessage = GetTabString() + "[TIME] " + timeString;
    WriteLog(logMessage);
}

void OutputCaller(const std::source_location location) {
    std::string logMessage = GetTabString() + "[CALLER] " + location.file_name() + ":" + std::to_string(location.line()) + " in " + location.function_name();
    WriteLog(logMessage);
}

void OutputSeparator() {
    std::string separator = GetTabString() + "----------------------------------------";
    WriteLog(separator);
}

void AddTab() {
    ++sTabCount;
}

void RemoveTab() {
    if (sTabCount > 0) {
        --sTabCount;
    }
}

} // namespace KashipanEngine::Log
