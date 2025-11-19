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
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <algorithm>

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
// スレッドごとのインデントとスコープフレーム
thread_local int sTabCount = -1;

// スコープフレーム
struct ScopeFrame {
    std::vector<std::string> scopes; // 外側→内側（namespace / class を区別しない）
    std::string functionName;
    int depth = -1;
    bool enteredFlushed = false;
    bool hadOutput = false;
};
thread_local std::vector<ScopeFrame> sScopeFrames;

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

//---------------- 非同期ロギング基盤 ----------------//
std::mutex sLogMutex;
std::condition_variable sLogCv;
std::deque<std::string> sLogQueue;
std::thread sLogThread;
std::atomic<bool> sStopRequested{false};
std::atomic<bool> sThreadRunning{false};

// 直接シンクへ書き込む（ワーカースレッドのみが使用）
void WriteToSinks(const std::string &formattedLine) {
    const auto &cfg = GetLogSettings();
    if (cfg.enableConsoleLogging) {
        OutputDebugStringA(formattedLine.c_str());
    }
    if (cfg.enableFileLogging && sLogFile.is_open()) {
        sLogFile << formattedLine;
    }
}

void LoggerWorker() {
    std::unique_lock<std::mutex> lock(sLogMutex);
    while (true) {
        sLogCv.wait(lock, [] { return !sLogQueue.empty() || sStopRequested.load(); });
        if (sStopRequested.load() && sLogQueue.empty()) {
            break;
        }
        auto line = std::move(sLogQueue.front());
        sLogQueue.pop_front();
        lock.unlock();
        WriteToSinks(line);
        lock.lock();
    }
}

//================= 統合: 以前の Logger/* ヘッダ内容 =================//

// 文字列の前後空白を除去
std::string_view Trim(std::string_view s) {
    const auto is_space = [](unsigned char c){ return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    size_t start = 0;
    while (start < s.size() && is_space(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && is_space(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

// 先頭の修飾子やキーワードを除去
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

// 後方の参照・ポインタ修飾を取り除く
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

// angle brackets の対応を取りながら、先頭のテンプレート引数列を抽出
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

// トップレベルのカンマでテンプレート引数を分割
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

// ユーザー定義型の候補かどうか（std:: で始まらず、英字を含む）
bool IsLikelyUserType(std::string_view s) {
    s = StripLeadingQualifiers(StripTrailingRefPtr(s));
    if (s.substr(0, 5) == "std::") return false;
    for (char c : s) {
        if (std::isalpha(static_cast<unsigned char>(c))) return true;
    }
    return false;
}

// namespace とクラス名を型名から抽出
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

// 情報保持用のスコープ情報構造体
struct ScopeInfo {
    // 自作部分の情報
    std::string namespaceName;  // 例: KashipanEngine
    std::string className;      // 例: GameEngine
    std::string functionName;   // 例: Initialize / operator() など（自作スコープに属する場合）

    // 外部部分の情報
    std::string externalNamespaceName;  // 例: std
    std::string externalFunctionName;   // 例: make_unique
    std::string rawSignature;           // source_location::function_name の生文字列
};

// source_location からスコープ情報を抽出（自作部分のみログに使用。std 部分は保持のみ）
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

// 指定文字列を n 回連結
std::string Repeat(const std::string &unit, int n) {
    if (n <= 0 || unit.empty()) return "";
    std::string out;
    out.reserve(unit.size() * static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) out += unit;
    return out;
}

// タブ文字列を取得
std::string GetTabString(int depth) {
    const auto &cfg = GetLogSettings();
    if (depth < 0) return "";
    return Repeat(cfg.nestedTabs, depth);
}

// 文字列置換ユーティリティ（${Key} を values[Key] で置換）
std::string FormatWithPlaceholders(std::string templ, const std::unordered_map<std::string, std::string>& values) {
    for (const auto &kv : values) {
        const std::string needle = "${" + kv.first + "}";
        size_t pos = 0;
        while ((pos = templ.find(needle, pos)) != std::string::npos) {
            templ.replace(pos, needle.size(), kv.second);
            pos += kv.second.size();
        }
    }
    return templ;
}

// ログ用の時間トークンを構築
std::unordered_map<std::string, std::string> BuildTimeTokens() {
    TimeRecord t = GetNowTime();
    auto pad2 = [](int v){ std::ostringstream os; os << std::setw(2) << std::setfill('0') << v; return os.str(); };
    auto pad4 = [](int v){ std::ostringstream os; os << std::setw(4) << std::setfill('0') << v; return os.str(); };

    std::unordered_map<std::string, std::string> tokens;
    tokens["Year"] = pad4(t.year);
    tokens["Month"] = pad2(t.month);
    tokens["Day"] = pad2(t.day);
    tokens["Hour"] = pad2(t.hour);
    tokens["Minute"] = pad2(t.minute);
    tokens["Second"] = pad2(static_cast<int>(t.second));
    tokens["BuildType"] = kBuildTypeString;
    return tokens;
}

// ログ行を構築
std::string BuildLogLine(const ScopeFrame* frame, LogSeverity severity, const std::string& message) {
    const auto &cfg = GetLogSettings();

    // TemplateLiteral を使ってプレースホルダ置換
    TemplateLiteral tpl(cfg.logMessageFormat);

    // 時刻系トークンを投入
    {
        auto timeTokens = BuildTimeTokens();
        for (const auto &kv : timeTokens) {
            tpl.Set(kv.first, kv.second);
        }
    }

    // 重大度・メッセージ
    tpl.Set("Severity", kLogSeverityLabel.at(severity));
    tpl.Set("Message", message);

    // インデントとスコープ情報
    const int depth = frame ? frame->depth : sTabCount;
    tpl.Set("NestedTabs", GetTabString(depth));

    const ScopeFrame* top = frame ? frame : GetTopScopeFrame();

    // scopes はテンプレート側の join 機能で連結するため、そのままベクタで渡す
    std::vector<std::string> outScopes;
    if (top) {
        const auto &sc = top->scopes;
        // ログ設定で namespace/class 出力がどちらも有効な場合はそのまま渡す
        if (cfg.enableOutputNamespace && cfg.enableOutputClass) {
            outScopes = sc;
        } else if (cfg.enableOutputNamespace) {
            // namespace 出力のみ有効な場合はログ設定の namespaces に含まれるものを渡す
            for (const auto &s : sc) {
                if (std::find(cfg.namespaces.begin(), cfg.namespaces.end(), s) != cfg.namespaces.end()) {
                    outScopes.push_back(s);
                }
            }
        } else if (cfg.enableOutputClass) {
            // class 出力のみ有効な場合はログ設定の namespaces に含まれないものを渡す
            for (const auto &s : sc) {
                if (std::find(cfg.namespaces.begin(), cfg.namespaces.end(), s) == cfg.namespaces.end()) {
                    outScopes.push_back(s);
                }
            }
        }

    }
    tpl.Set("scopes", outScopes);

    tpl.Set("function",  top ? top->functionName  : std::string());

    // 旧プレースホルダ互換（空、またはスコープ末尾を class として提供）
    if (top && !top->scopes.empty()) {
        tpl.Set("class", top->scopes.back());
        // namespace は曖昧なので、先頭〜末尾-1 を連結して提供
        std::string ns;
        if (!outScopes.empty()) {
            // outScopes から namespace 相当を再構築（末尾を class とみなす）
            if (outScopes.size() > 1) {
                for (size_t i = 0; i + 1 < outScopes.size(); ++i) {
                    if (i) ns += "::";
                    ns += outScopes[i];
                }
            }
        }
        tpl.Set("namespace", ns);
    } else {
        tpl.Set("class", std::string());
        tpl.Set("namespace", std::string());
    }

    std::string line = tpl.Render();
    line += '\n';
    return line;
}

// ログ出力（非同期キューへ積む）
void WriteLog(const std::string &formattedLine) {
    // ここでは IO を行わず、ワーカースレッドに委譲
    {
        std::lock_guard<std::mutex> lock(sLogMutex);
        sLogQueue.emplace_back(formattedLine);
    }
    sLogCv.notify_one();
}

// スコープ "Entering" の遅延出力をフラッシュ
void FlushPendingScopeEnters() {
    if (sScopeFrames.empty()) return;
    size_t first = 0;
    while (first < sScopeFrames.size() && sScopeFrames[first].enteredFlushed) ++first;
    for (size_t i = first; i < sScopeFrames.size(); ++i) {
        auto &frame = sScopeFrames[i];
        WriteLog(BuildLogLine(&frame, LogSeverity::Debug, "Entering scope."));
        frame.enteredFlushed = true;
    }
}

// ログファイル名を構築
std::string BuildLogFileName() {
    const auto &cfg = GetLogSettings();
    TemplateLiteral tpl(cfg.logFileFormat);

    auto tokens = BuildTimeTokens();
    for (const auto &kv : tokens) tpl.Set(kv.first, kv.second);

    std::string name = tpl.Render();
    if (name.size() < 4 || name.substr(name.size() - 4) != ".log") {
        name += ".log";
    }
    return name;
}

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

//================= 統合ここまで =================//

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

    // ログスレッド起動
    sStopRequested.store(false);
    sThreadRunning.store(true);
    sLogThread = std::thread(LoggerWorker);

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

    // スレッド終了要求し、キューを空にしてから終了
    {
        std::lock_guard<std::mutex> lock(sLogMutex);
        sStopRequested.store(true);
    }
    sLogCv.notify_all();
    if (sLogThread.joinable()) {
        sLogThread.join();
    }
    sThreadRunning.store(false);

    if (sLogFile.is_open()) {
        sLogFile.close();
    }
    sLoggerInitialized = false;
}

void ForceShutdownLogger(PasskeyForCrashHandler) {
    if (!sLoggerInitialized) return;

    // 可能ならワーカースレッドを止める
    {
        std::lock_guard<std::mutex> lock(sLogMutex);
        sStopRequested.store(true);
        sLogQueue.clear(); // 即時終了を優先
    }
    sLogCv.notify_all();
    if (sLogThread.joinable()) {
        // ここでの join は最善努力
        sLogThread.join();
    }
    sThreadRunning.store(false);

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
