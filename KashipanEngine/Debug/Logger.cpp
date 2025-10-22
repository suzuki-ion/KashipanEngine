#include "Logger.h"
#include "EngineSettings.h"
#include "Utilities/TimeUtils.h"
#include <Windows.h>
#include <regex>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <string_view>
#include <cctype>

namespace KashipanEngine {

namespace {
/// @brief ログファイルストリーム
std::ofstream sLogFile;
/// @brief ビルドの種類の文字列
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
/// @brief ログのタブ数
int sTabCount = -1;
/// @brief タブの空白の数
const int kTabSpaceCount = 4;
/// @brief プレフィックススタック
std::vector<std::string> sPrefixStack = {};
/// @brief ログ大分類文字列マップ
const std::unordered_map<LogDomain, std::string> kLogDomainToString = {
    { LogDomain::GameEngine, "[GAME ENIGNE]" },
    { LogDomain::Application, "[APPLICATION]" },
};
/// @brief ログ情報文字列マップ
const std::unordered_map<LogSeverity, std::string> kLogSeverityToString = {
    { LogSeverity::Debug, "[DEBUG]" },
    { LogSeverity::Info, "[INFO]" },
    { LogSeverity::Warning, "[WARNING]" },
    { LogSeverity::Error, "[ERROR]" },
    { LogSeverity::Critical, "[CRITICAL]" },
};

/// @brief 文字列の前後空白を除去
std::string_view Trim(std::string_view s) {
    const auto is_space = [](unsigned char c){ return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    size_t start = 0;
    while (start < s.size() && is_space(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && is_space(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

/// @brief 先頭の修飾子やキーワードを除去
std::string_view StripLeadingQualifiers(std::string_view s) {
    s = Trim(s);
    auto starts_with = [](std::string_view a, std::string_view b){ return a.substr(0, b.size()) == b; };
    bool changed = true;
    while (changed) {
        changed = false;
        if (starts_with(s, "const ")) { s.remove_prefix(6); changed = true; }
        if (starts_with(s, "volatile ")) { s.remove_prefix(9); changed = true; }
        if (starts_with(s, "class ")) { s.remove_prefix(6); changed = true; }
        if (starts_with(s, "struct ")) { s.remove_prefix(7); changed = true; }
        if (starts_with(s, "enum ")) { s.remove_prefix(5); changed = true; }
        if (starts_with(s, "union ")) { s.remove_prefix(6); changed = true; }
    }
    return Trim(s);
}

/// @brief 後方の参照・ポインタ修飾を取り除く
std::string_view StripTrailingRefPtr(std::string_view s) {
    s = Trim(s);
    while (!s.empty()) {
        char back = s.back();
        if (back == '&' || back == '*') {
            s.remove_suffix(1);
            s = Trim(s);
        } else {
            break;
        }
    }
    return s;
}

/// @brief angle brackets の対応を取りながら、先頭のテンプレート引数列を抽出
bool ExtractFirstTemplateArgList(std::string_view s, std::string_view &outArgs) {
    size_t lt = s.find('<');
    if (lt == std::string_view::npos) return false;
    int depth = 0;
    for (size_t i = lt; i < s.size(); ++i) {
        char c = s[i];
        if (c == '<') ++depth;
        else if (c == '>') {
            --depth;
            if (depth == 0) {
                outArgs = s.substr(lt + 1, i - (lt + 1));
                return true;
            }
        }
    }
    return false;
}

/// @brief トップレベルのカンマでテンプレート引数を分割
std::vector<std::string_view> SplitTemplateArgs(std::string_view args) {
    std::vector<std::string_view> result;
    int depth = 0;
    size_t start = 0;
    for (size_t i = 0; i < args.size(); ++i) {
        char c = args[i];
        if (c == '<') ++depth;
        else if (c == '>') --depth;
        else if (c == ',' && depth == 0) {
            result.emplace_back(Trim(args.substr(start, i - start)));
            start = i + 1;
        }
    }
    if (start < args.size()) result.emplace_back(Trim(args.substr(start)));
    return result;
}

/// @brief ユーザー定義型の候補かどうか（std:: で始まらず、英字を含む）
bool IsLikelyUserType(std::string_view s) {
    s = StripLeadingQualifiers(StripTrailingRefPtr(s));
    if (s.substr(0, 5) == "std::") return false;
    for (char c : s) {
        if (std::isalpha(static_cast<unsigned char>(c))) return true;
    }
    return false;
}

/// @brief namespace とクラス名を型名から抽出
void ParseNamespaceAndClass(std::string_view typeName, std::string &outNs, std::string &outClass) {
    typeName = StripLeadingQualifiers(StripTrailingRefPtr(typeName));
    // テンプレートの本体名を抽出
    std::string_view cleaned = typeName;
    size_t lt = cleaned.find('<');
    if (lt != std::string_view::npos) cleaned = cleaned.substr(0, lt);
    cleaned = Trim(cleaned);
    if (cleaned.size() >= 2 && cleaned.substr(0, 2) == "::") cleaned.remove_prefix(2);

    size_t last = cleaned.rfind("::");
    if (last != std::string_view::npos) {
        outNs = std::string(Trim(cleaned.substr(0, last)));
        outClass = std::string(Trim(cleaned.substr(last + 2)));
    } else {
        outNs.clear();
        outClass = std::string(cleaned);
    }
}

/// 情報保持用のスコープ情報構造体
struct ScopeInfo {
    // 出力用（自作部分のみ）
    std::string namespaceName; // 例: KashipanEngine
    std::string className;     // 例: GameEngine
    std::string functionName;  // 例: Initialize / operator() など（自作スコープに属する場合）

    // 保持用（ログには出さない）
    std::string externalNamespaceName; // 例: std
    std::string externalFunctionName;  // 例: make_unique
    std::string rawSignature;          // source_location::function_name の生文字列
};

/// @brief source_location からスコープ情報を抽出（自作部分のみログに使用。std 部分は保持のみ）
ScopeInfo GetScopeInfo(const std::source_location &location) {
    ScopeInfo info{};
    std::string sig = location.function_name();
    info.rawSignature = sig;

    // パラメータ以降を削除
    size_t lparen = sig.find('(');
    if (lparen != std::string::npos) sig = sig.substr(0, lparen);

    // MSVC の呼出規約トークン以降を使用
    size_t convPos = sig.find("__cdecl");
    if (convPos != std::string::npos) {
        sig = sig.substr(convPos + 7); // "__cdecl" の長さ
    } else {
        // フォールバック: 直近の空白以降を使用（戻り値型を除去）
        size_t lastSpace = sig.rfind(' ');
        if (lastSpace != std::string::npos) {
            sig = sig.substr(lastSpace + 1);
        }
    }

    // トリムと先頭の :: 除去
    std::string_view view = Trim(sig);
    if (view.size() >= 2 && view.substr(0, 2) == "::") view.remove_prefix(2);

    // まず先頭トークン（例: std::make_unique）を確認
    std::string_view argsView;
    bool hasTemplateArgs = ExtractFirstTemplateArgList(view, argsView);

    // 先頭スコープ（< の手前）
    std::string_view head = view;
    size_t headEnd = head.find('<');
    if (headEnd != std::string_view::npos) head = head.substr(0, headEnd);
    head = Trim(head);

    // "std::..." を検出
    bool headIsStd = head.size() >= 5 && head.substr(0, 5) == "std::";
    if (headIsStd) {
        // 保持のみ（ログには出さない）
        info.externalNamespaceName = "std";
        size_t fnSep = head.rfind("::");
        if (fnSep != std::string::npos) {
            info.externalFunctionName = std::string(head.substr(fnSep + 2));
        } else {
            info.externalFunctionName = std::string(head);
        }

        // テンプレート引数からユーザー型を抽出
        if (hasTemplateArgs) {
            auto tokens = SplitTemplateArgs(argsView);
            for (auto t : tokens) {
                t = StripLeadingQualifiers(StripTrailingRefPtr(t));
                if (!IsLikelyUserType(t)) continue;
                if (t.substr(0, 5) == "std::") continue;
                ParseNamespaceAndClass(t, info.namespaceName, info.className);
                break;
            }
        }

        return info;
    }

    // 通常ケース: 自作の関数/メンバ。スコープ区切りで分割
    std::vector<std::string_view> parts;
    size_t start = 0;
    while (start <= view.size()) {
        size_t pos = view.find("::", start);
        if (pos == std::string_view::npos) {
            parts.emplace_back(view.substr(start));
            break;
        } else {
            parts.emplace_back(view.substr(start, pos - start));
            start = pos + 2;
        }
    }

    if (!parts.empty()) {
        // 関数名（テンプレート名を除去）
        std::string_view funcToken = Trim(parts.back());
        size_t lt2 = funcToken.find('<');
        if (lt2 != std::string_view::npos) funcToken = funcToken.substr(0, lt2);
        info.functionName = std::string(funcToken);

        if (parts.size() >= 2) {
            info.className = std::string(Trim(parts[parts.size() - 2]));
            if (info.className.rfind("std", 0) == 0) {
                info.className.clear();
            }
        }
        if (parts.size() >= 3) {
            std::string ns;
            for (size_t i = 0; i + 2 < parts.size(); ++i) {
                std::string token = std::string(Trim(parts[i]));
                if (token.rfind("std", 0) == 0) continue;
                if (!ns.empty()) ns += "::";
                ns += token;
            }
            info.namespaceName = std::move(ns);
        }
    }

    return info;
}

/// @brief タブ文字列を取得
std::string GetTabString() {
    if (sTabCount < 0) {
        return "";
    }
    return std::string(sTabCount * kTabSpaceCount, ' ');
}

/// @brief プレフィックス文字列を取得
std::string GetPrefixString() {
    if (sPrefixStack.empty()) {
        return "";
    }
    return sPrefixStack.back();
}

/// @brief ログを書き込む
void WriteLog(const std::string &logMessage) {
    std::string message = "[" + GetNowTimeString("%Y-%m-%d %H:%M:%S") + "] ";
    message += GetPrefixString() + ": ";
    message += logMessage + '\n';
    // コンソールに出力
    OutputDebugStringA(message.c_str());
    // ファイルに出力
    if (sLogFile.is_open()) {
        sLogFile << message;
    }
}

/// @brief ファイルストリーム初期化用クラス
class FileStreamInitializer {
public:
    FileStreamInitializer() {
        const std::string currentDir = std::filesystem::current_path().string();
        const std::string logDir = currentDir + "/" + outputDir_;
        const std::string logFilePath = logDir + "/" + kBuildTypeString + " " + GetNowTimeString("%Y-%m-%d_%H-%M%S") + ".log";
        std::filesystem::create_directories(logDir);
        sLogFile.open(logFilePath, std::ios::out);
        if (!sLogFile) {
            assert(false && "ログファイルのオープンに失敗しました。");
            return;
        }
        WriteLog("----- Log Start -----");
        WriteLog("Log File: " + logFilePath);
    }
    ~FileStreamInitializer() {
        if (sLogFile.is_open()) {
            WriteLog("----- Log End -----");
            sLogFile.close();
        }
    }
private:
    const std::string outputDir_ = "Logs";
} sFileStreamInitializer;
} // namespace

void Log(const std::string &logText, LogSeverity severity) {
    std::string logMessage;
    logMessage += kLogSeverityToString.at(severity) + " "; 
    logMessage += logText;
    WriteLog(logMessage);
}

void LogSeparator() {
    std::string separator = "--------------------------------------------------";
    WriteLog(separator);
}

void LogScope::PushPrefix(const std::source_location &location) {
    ++sTabCount;
    ScopeInfo info = GetScopeInfo(location);
    std::string prefix;
    prefix += GetTabString();
    if (!info.namespaceName.empty()) {
        prefix += "[ namespace " + info.namespaceName + " ]";
    }
    if (!info.className.empty()) {
        prefix += "[ class " + info.className + " ]";
    }
    if (!info.functionName.empty()) {
        prefix += "[ function " + info.functionName + " ]";
    }
    sPrefixStack.push_back(prefix);
}

void LogScope::PopPrefix() {
    if (sPrefixStack.size() <= 0) {
        return;
    }
    sPrefixStack.pop_back();
    --sTabCount;
}

} // namespace KashipanEngine
