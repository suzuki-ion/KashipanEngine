#include "Utilities/DevelopmentAssistant.h"
#include "Utilities/DevelopmentAssistantTools.h"
#define NOMINMAX
#include <Windows.h>
#include <json.hpp>
#include <algorithm>
#include <chrono>
#include <cwctype>
#include <functional>
#include <stdexcept>
#include <fstream>
#include <regex>

namespace KashipanEngine::DevelopmentAI {
namespace {
using Json = nlohmann::json;
constexpr size_t OutputLimit = 65536;
std::wstring Wide(const std::string &s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (!n) throw std::runtime_error("Invalid UTF-8");
    std::wstring result(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), result.data(), n);
    return result;
}
std::string Utf8(const std::wstring &s) {
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), n, nullptr, nullptr);
    return result;
}
struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle() = default;
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
};
std::vector<wchar_t> Environment() {
    // Inherit normal OS variables, but never allow API billing or a different inference provider.
    auto block = GetEnvironmentStringsW();
    if (!block) throw std::runtime_error("Environment unavailable");
    std::vector<wchar_t> result;
    for (auto p = block; *p; p += wcslen(p) + 1) {
        std::wstring entry(p);
        auto separator = entry.find(L'=', entry.front() == L'=' ? 1 : 0);
        if (!IsBillingEnvironment(entry.substr(0, separator))) {
            result.insert(result.end(), entry.begin(), entry.end()); result.push_back(0);
        }
    }
    FreeEnvironmentStringsW(block);
    result.push_back(0);
    if (result.size() == 1) result.push_back(0);
    return result;
}
std::filesystem::path Resolve(Provider provider, const std::string &configured) {
    if (!configured.empty()) {
        auto path = std::filesystem::path(Wide(configured));
        if (path.is_absolute() && path.extension() == L".exe" && std::filesystem::is_regular_file(path)) return path;
        return {};
    }
    wchar_t buffer[32768]{};
    const wchar_t *name = provider == Provider::Claude ? L"claude.exe" : L"codex.exe";
    auto length = SearchPathW(nullptr, name, nullptr, 32768, buffer, nullptr);
    if (length && length < 32768) return buffer;
    if (provider == Provider::Claude) {
        if (GetEnvironmentVariableW(L"USERPROFILE", buffer, 32768)) {
            auto candidate = std::filesystem::path(buffer) / L".local/bin/claude.exe";
            if (std::filesystem::is_regular_file(candidate)) return candidate;
        }
    } else if (GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, 32768)) {
        auto root = std::filesystem::path(buffer) / L"OpenAI/Codex/bin";
        std::error_code ec;
        std::filesystem::path newest;
        std::filesystem::file_time_type newestTime{};
        for (std::filesystem::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
            auto candidate = it->path() / L"codex.exe";
            if (!std::filesystem::is_regular_file(candidate, ec)) { ec.clear(); continue; }
            auto time = std::filesystem::last_write_time(candidate, ec);
            if (!ec && (newest.empty() || time > newestTime)) { newest = candidate; newestTime = time; }
        }
        return newest;
    }
    return {};
}
// Prompt is sent through stdin, never through a shell or command line. Capture is bounded.
int Run(const std::filesystem::path &exe, const std::vector<std::wstring> &args,
    const std::filesystem::path &cwd, const std::string &input, std::atomic<bool> &cancel,
    unsigned timeoutSeconds, const std::function<void(const std::string &)> &line,
    const std::function<std::string(const std::string &)> &reply = {}, const std::function<bool()> &complete = {}) {
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    Handle read, write, stdinRead, stdinWrite, job, process, thread;
    if (!CreatePipe(&read.value, &write.value, &security, 0) ||
        !CreatePipe(&stdinRead.value, &stdinWrite.value, &security, 0)) throw std::runtime_error("Pipe creation failed");
    SetHandleInformation(read.value, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdinWrite.value, HANDLE_FLAG_INHERIT, 0);
    job.value = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job.value || !SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
        throw std::runtime_error("Process job unavailable");
    std::wstring command = QuoteArgument(exe.wstring());
    for (const auto &arg : args) command += L" " + QuoteArgument(arg);
    auto environment = Environment();
    STARTUPINFOEXW startup{}; startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdOutput = write.value; startup.StartupInfo.hStdError = write.value; startup.StartupInfo.hStdInput = stdinRead.value;
    SIZE_T attributeBytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
    std::vector<unsigned char> attributes(attributeBytes);
    startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    if (!InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &attributeBytes)) throw std::runtime_error("Handle isolation unavailable");
    struct AttributeCleanup {
        LPPROC_THREAD_ATTRIBUTE_LIST list;
        ~AttributeCleanup() { DeleteProcThreadAttributeList(list); }
    } attributeCleanup{startup.lpAttributeList};
    HANDLE inherited[] = {write.value, stdinRead.value};
    if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        inherited, sizeof(inherited), nullptr, nullptr)) throw std::runtime_error("Handle isolation failed");
    PROCESS_INFORMATION info{};
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT, environment.data(),
        cwd.empty() ? nullptr : cwd.c_str(), &startup.StartupInfo, &info)) throw std::runtime_error("CLI process could not start");
    process.value = info.hProcess; thread.value = info.hThread;
    if (!AssignProcessToJobObject(job.value, process.value)) {
        TerminateProcess(process.value, 1); throw std::runtime_error("CLI process could not be isolated");
    }
    CloseHandle(write.value); write.value = nullptr;
    CloseHandle(stdinRead.value); stdinRead.value = nullptr;
    ResumeThread(thread.value);
    // Concurrent writer avoids deadlocks if the child emits output while reading a large prompt.
    std::thread writer([&] {
        size_t offset = 0;
        while (offset < input.size()) {
            DWORD written = 0;
            if (!WriteFile(stdinWrite.value, input.data() + offset,
                static_cast<DWORD>(std::min<size_t>(4096, input.size() - offset)), &written, nullptr) || !written) break;
            offset += written;
        }
        if (!reply) { CloseHandle(stdinWrite.value); stdinWrite.value = nullptr; }
    });
    const auto start = std::chrono::steady_clock::now();
    std::string pending;
    bool discarding = false;
    bool stopped = false;
    auto consume = [&](const std::string &value) {
        line(value);
        if (reply) {
            auto response = reply(value);
            if (!response.empty()) {
                DWORD written = 0;
                if (response.size() > 4096 || !WriteFile(stdinWrite.value, response.data(), static_cast<DWORD>(response.size()), &written, nullptr) || written != response.size())
                    throw std::runtime_error("CLI protocol write failed");
            }
        }
    };
    try {
        for (;;) {
            if (complete && complete()) break;
            if (cancel.load() || std::chrono::steady_clock::now() - start > std::chrono::seconds(timeoutSeconds)) {
                stopped = true; TerminateJobObject(job.value, 1); break;
            }
            DWORD available = 0;
            if (PeekNamedPipe(read.value, nullptr, 0, nullptr, &available, nullptr) && available) {
                char chunk[4096]; DWORD count = 0;
                if (ReadFile(read.value, chunk, std::min<DWORD>(available, sizeof(chunk)), &count, nullptr)) {
                    for (DWORD i = 0; i < count; ++i) {
                        if (chunk[i] == '\n') { if (!discarding) consume(pending); pending.clear(); discarding = false; }
                        else if (!discarding) {
                            if (pending.size() >= 1024 * 1024) { pending.clear(); discarding = true; }
                            else pending.push_back(chunk[i]);
                        }
                    }
                }
            } else if (WaitForSingleObject(process.value, 0) == WAIT_OBJECT_0) break;
            else Sleep(20);
        }
        if (!pending.empty() && !discarding && !stopped) consume(pending);
    } catch (...) {
        TerminateJobObject(job.value, 1); writer.join(); throw;
    }
    TerminateJobObject(job.value, 1); // also end any child work after the owning turn exits
    writer.join();
    DWORD exitCode = 1; GetExitCodeProcess(process.value, &exitCode);
    if (complete && complete() && !stopped && !cancel) return 0;
    return stopped ? -1 : static_cast<int>(exitCode);
}
Account Probe(Provider provider, const std::string &configured, std::atomic<bool> &cancel) {
    Account account;
    const auto exe = Resolve(provider, configured);
    if (exe.empty()) { account.message = "公式CLIの.exeが見つかりません。絶対パスを指定してください。"; return account; }
    account.installed = true; account.executable = Utf8(exe.wstring());
    std::string result;
    const auto args = provider == Provider::Claude ? std::vector<std::wstring>{L"auth", L"status", L"--json"}
        : std::vector<std::wstring>{L"login", L"status"};
    int exitCode = Run(exe, args, {}, {}, cancel, 20, [&](const auto &s) {
        if (result.size() + s.size() < OutputLimit) result += s + "\n";
    });
    account.subscription = IsSubscription(provider, result, exitCode);
    account.message = account.subscription ? "契約アカウントでログイン済み" :
        "契約アカウントのログインを確認できません（APIキー方式はこの機能では使用しません）。";
    return account;
}
void Discover(Provider provider, Account &account, std::atomic<bool> &cancel) {
    account.models.clear();
    if (!account.subscription) { account.modelsStatus = "契約アカウントでログインしてください。"; return; }
    bool complete = false, failed = false;
    size_t pages = 0;
    std::vector<Model> models;
    auto args = provider == Provider::Claude ? RequestArguments(provider, Mode::Suggest, "") :
        std::vector<std::wstring>{L"app-server", L"--listen", L"stdio://", L"-c", L"model_provider=\"openai\""};
    std::string initial;
    if (provider == Provider::Claude) {
        args.insert(args.end(), {L"--input-format", L"stream-json"});
        initial = R"({"type":"control_request","request_id":"catalog","request":{"subtype":"initialize"}})" "\n";
    } else initial = R"({"id":1,"method":"initialize","params":{"clientInfo":{"name":"kashipan_engine","title":"KashipanEngine","version":"1"}}})" "\n";
    const int code = Run(Wide(account.executable), args, {}, initial, cancel, 45, [](const auto &) {}, [&](const auto &line) -> std::string {
        try {
            auto j = Json::parse(line);
            if (provider == Provider::Claude) {
                if (j.value("type", std::string{}) != "control_response" || !j.contains("response")) return {};
                const auto &r = j.at("response");
                if (r.value("request_id", std::string{}) != "catalog") return {};
                complete = true;
                if (r.value("subtype", std::string{}) != "success") { failed = true; return {}; }
                models = ParseModels(provider, r.at("response").dump());
            } else {
                if (!j.contains("id")) return {};
                if (j.contains("error")) { complete = failed = true; return {}; }
                if (j.at("id") == 1) return R"({"method":"initialized","params":{}})" "\n"
                    R"({"id":2,"method":"model/list","params":{"limit":100,"includeHidden":false}})" "\n";
                if (j.at("id") != 2) return {};
                auto result = j.at("result");
                auto page = ParseModels(provider, result.dump());
                models.insert(models.end(), page.begin(), page.end());
                if (++pages > 10 || models.size() > 512) { complete = failed = true; return {}; }
                if (result.contains("nextCursor") && result["nextCursor"].is_string() && !result["nextCursor"].get<std::string>().empty()) {
                    return Json{{"id", 2}, {"method", "model/list"}, {"params", {{"limit", 100}, {"includeHidden", false}, {"cursor", result["nextCursor"]}}}}.dump() + "\n";
                }
                complete = true;
            }
        } catch (...) {
            // Non-JSON stderr is harmless; malformed JSON protocol must not publish a partial catalog.
            if (Json::accept(line)) { complete = failed = true; }
        }
        return {};
    }, [&] { return complete; });
    if (code == 0 && complete && !failed && !cancel && !models.empty()) {
        for (auto &model : models) if (std::none_of(account.models.begin(), account.models.end(), [&](const auto &m) { return m.id == model.id; }))
            account.models.push_back(std::move(model));
        account.modelsStatus = "モデル一覧を更新しました。";
    } else account.modelsStatus = cancel ? "一覧取得を中止しました。" : "モデル一覧を取得できませんでした。CLI・ログイン・組織設定を確認してください。";
}
}

std::vector<Model> ParseModels(Provider provider, const std::string &json) {
    const auto j = Json::parse(json);
    const auto &items = j.at(provider == Provider::Claude ? "models" : "data");
    if (!items.is_array() || items.size() > 512) throw std::runtime_error("Invalid catalog");
    std::vector<Model> result;
    for (const auto &item : items) {
        if (!item.is_object() || item.value("hidden", false)) continue;
        const auto id = item.value(provider == Provider::Claude ? "value" : "model", std::string{});
        if (id.empty() || id == "default") continue;
        try { RequestArguments(provider, Mode::Suggest, id); } catch (...) { continue; }
        if (std::any_of(result.begin(), result.end(), [&](const auto &m) { return m.id == id; })) continue;
        result.push_back({id, item.value("displayName", id).substr(0, 256), item.value("description", std::string{}).substr(0, 2048)});
    }
    return result;
}

bool IsSubscription(Provider provider, const std::string &status, int exitCode) {
    if (exitCode != 0) return false;
    if (provider == Provider::Codex) return status.find("Logged in using ChatGPT") != std::string::npos &&
        status.find("API key") == std::string::npos;
    try {
        const auto j = Json::parse(status);
        return j.value("loggedIn", false) && j.value("authMethod", std::string{}) == "claude.ai" &&
            j.value("apiProvider", std::string{}) == "firstParty";
    } catch (...) { return false; }
}
std::wstring QuoteArgument(const std::wstring &arg) {
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') { ++slashes; continue; }
        result.append(slashes * (c == L'\"' ? 2 : 1), L'\\'); slashes = 0;
        if (c == L'\"') result += L'\\';
        result += c;
    }
    result.append(slashes * 2, L'\\'); result += L'\"'; return result;
}
bool IsBillingEnvironment(const std::wstring &name) {
    std::wstring upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](wchar_t c) { return static_cast<wchar_t>(towupper(c)); });
    return upper.starts_with(L"ANTHROPIC_") || upper.starts_with(L"CLAUDE_CODE_USE_") ||
        upper == L"CLAUDE_CODE_OAUTH_TOKEN" || upper.starts_with(L"OPENAI_") ||
        upper == L"CODEX_API_KEY" || upper == L"CODEX_ACCESS_TOKEN" ||
        upper == L"CODEX_PERMISSION_PROFILE" || upper == L"CODEX_SESSION_ID" || upper == L"CODEX_THREAD_ID" ||
        upper == L"CODEX_APP_TOOLS_PIPE_PATH" || upper == L"CODEX_INTERNAL_ORIGINATOR_OVERRIDE" ||
        upper == L"CODEX_TASK_WORKSPACE_VERIFYING_IDENTITY" || upper == L"CODEX_WINDOWS_SANDBOX_PACKAGE_FAMILY";
}
std::vector<std::wstring> WindowsSandboxArguments(const std::string &backend) {
    std::string fallback = "elevated";
    if (backend == "auto") {
        // Keep billing/provider configuration isolated, but preserve the user's native sandbox choice.
        wchar_t directory[32768]{};
        std::filesystem::path root;
        if (GetEnvironmentVariableW(L"CODEX_HOME", directory, 32768)) root = directory;
        else if (GetEnvironmentVariableW(L"USERPROFILE", directory, 32768)) root = std::filesystem::path(directory) / L".codex";
        std::ifstream config(root / L"config.toml");
        std::string line; bool windows = false; size_t bytes = 0;
        const std::regex section(R"(^\s*\[windows\]\s*(?:#.*)?$)");
        const std::regex setting(R"regex(^\s*sandbox\s*=\s*["'](elevated|unelevated|mxc)["']\s*(?:#.*)?$)regex");
        while (std::getline(config, line) && (bytes += line.size()) < 1024 * 1024) {
            auto first = line.find_first_not_of(" \t\r");
            if (first != std::string::npos && line[first] == '[') windows = std::regex_match(line, section);
            std::smatch match;
            if (windows && std::regex_match(line, match, setting)) fallback = match[1].str();
        }
    } else if (backend == "mxc" || backend == "elevated" || backend == "unelevated") fallback = backend;
    else throw std::runtime_error("Invalid Windows sandbox backend");
    return {L"-c", L"features.prefer_mxc=" + std::wstring(backend == "auto" ? L"true" : L"false"),
        L"-c", L"windows.sandbox=\"" + Wide(fallback) + L"\""};
}
std::vector<std::wstring> RequestArguments(Provider provider, Mode mode, const std::string &model) {
    if (model.size() > 128 || std::any_of(model.begin(), model.end(), [](unsigned char c) {
        return !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9') && c != '-' && c != '_' && c != '.' && c != '[' && c != ']';
    })) throw std::runtime_error("Invalid model name");
    std::vector<std::wstring> args;
    if (provider == Provider::Claude) {
        args = {L"-p", L"--restricted", L"--output-format", L"stream-json", L"--verbose", L"--no-session-persistence",
            L"--setting-sources", L"", L"--strict-mcp-config", L"--mcp-config", L"{\"mcpServers\":{}}",
            L"--permission-mode", L"dontAsk", L"--tools", mode == Mode::EditFiles ? L"Read,Edit,Write,Glob,Grep" : L"",
            L"--allowedTools", mode == Mode::EditFiles ? L"Read,Edit,Write,Glob,Grep" : L""};
    } else {
        args = {L"exec", L"--json", L"--ephemeral", L"--ignore-user-config", L"--color", L"never",
            L"--skip-git-repo-check", L"-c", L"model_provider=\"openai\"", L"-c", L"approval_policy=\"never\"",
            L"--sandbox", mode == Mode::EditFiles ? L"workspace-write" : L"read-only"};
    }
    if (!model.empty()) { args.push_back(L"--model"); args.push_back(Wide(model)); }
    if (provider == Provider::Codex) args.push_back(L"-");
    return args;
}
void ConsumeEvent(Provider provider, const std::string &line, Snapshot &state, bool &terminal) {
    try {
        const auto j = Json::parse(line);
        const auto type = j.value("type", std::string{});
        auto append = [&](const std::string &s) {
            if (state.output.size() < OutputLimit) state.output += s.substr(0, OutputLimit - state.output.size()) + "\n";
        };
        if (provider == Provider::Claude) {
            if (type == "assistant" && j.contains("message")) {
                for (const auto &block : j.at("message").value("content", Json::array()))
                    if (block.value("type", std::string{}) == "text") append(block.value("text", std::string{}));
            }
            if (type == "result") {
                terminal = true;
                state.success = !j.value("is_error", true) && j.value("subtype", std::string{}) == "success";
                if (j.contains("result") && j["result"].is_string()) state.output = j["result"].get<std::string>().substr(0, OutputLimit);
                if (!state.success && state.output.empty()) state.output = j.value("subtype", std::string{"CLI error"});
            }
        } else {
            if (type == "item.completed" && j.contains("item")) {
                const auto &item = j.at("item");
                if (item.value("type", std::string{}) == "agent_message") append(item.value("text", std::string{}));
                if (item.value("type", std::string{}) == "command_execution" && item.value("status", std::string{}) == "failed") {
                    const auto detail = item.value("aggregated_output", std::string{"Command execution failed"});
                    if (state.diagnostics.size() < 16384) state.diagnostics += detail.substr(0, 16384 - state.diagnostics.size()) + "\n";
                }
            }
            if (type == "turn.completed") { terminal = true; state.success = true; }
            if (type == "turn.failed" || type == "error") {
                terminal = true; state.success = false;
                if (j.contains("error") && j["error"].is_object()) append(j["error"].value("message", std::string{"CLI error"}));
                else append(j.value("message", std::string{"CLI error"}));
            }
        }
    } catch (...) { /* stderr and incompatible protocol data never become executable instructions */ }
}
Session::~Session() { Cancel(); Join(); }
Snapshot Session::Get() const { std::lock_guard lock(mutex_); return state_; }
void Session::Cancel() { cancel_ = true; }
void Session::Join() { if (worker_.joinable()) worker_.join(); }
void Session::Refresh(const std::string &claudePath, const std::string &codexPath) {
    if (Get().busy) return;
    Join(); cancel_ = false;
    { std::lock_guard lock(mutex_); state_.busy = true; state_.status = "CLIとログイン状態を確認中…"; }
    worker_ = std::thread([this, claudePath, codexPath] {
        Account claude, codex;
        try { claude = Probe(Provider::Claude, claudePath, cancel_); if (!cancel_) Discover(Provider::Claude, claude, cancel_); } catch (...) { claude.modelsStatus = "Claudeの一覧取得に失敗しました。"; }
        try { if (!cancel_) { codex = Probe(Provider::Codex, codexPath, cancel_); Discover(Provider::Codex, codex, cancel_); } } catch (...) { codex.modelsStatus = "Codexの一覧取得に失敗しました。"; }
        std::lock_guard lock(mutex_);
        state_.claude = std::move(claude); state_.codex = std::move(codex);
        state_.busy = false; state_.status = cancel_ ? "確認を中止しました。" : "ログイン状態を更新しました。";
    });
}
void Session::RefreshModels(Provider provider, const std::string &path) {
    if (Get().busy) return;
    Join(); cancel_ = false;
    { std::lock_guard lock(mutex_); state_.busy = true; state_.status = "モデル一覧を更新中…"; }
    worker_ = std::thread([this, provider, path] {
        Account account;
        try { account = Probe(provider, path, cancel_); Discover(provider, account, cancel_); }
        catch (...) { account.modelsStatus = "モデル一覧を取得できませんでした。"; }
        std::lock_guard lock(mutex_);
        (provider == Provider::Claude ? state_.claude : state_.codex) = std::move(account);
        state_.busy = false; state_.status = cancel_ ? "中止しました。" : "モデル一覧の更新を終了しました。";
    });
}
bool Session::Start(Provider provider, Mode mode, const std::string &model,
    const std::filesystem::path &project, const std::string &prompt, const std::string &windowsBackend,
    std::shared_ptr<EditorTools> tools) {
    auto snapshot = Get();
    if (snapshot.busy || project.empty() || !std::filesystem::is_directory(project) || prompt.empty() || prompt.size() > 262144) return false;
    auto account = provider == Provider::Claude ? snapshot.claude : snapshot.codex;
    if (!account.subscription || (mode == Mode::EditScene) != static_cast<bool>(tools)) return false;
    std::vector<std::wstring> args;
    std::vector<std::wstring> windows;
    try {
        args = RequestArguments(provider, mode, model);
        if (tools) {
            if (provider == Provider::Claude) {
                const Json server{{"mcpServers", {{"kashipan_editor", {{"type", "http"}, {"url", tools->Url()},
                    {"headers", {{"Authorization", "Bearer " + tools->Token()}}}}}}}};
                *std::next(std::find(args.begin(), args.end(), L"--mcp-config")) = Wide(server.dump());
                *std::next(std::find(args.begin(), args.end(), L"--allowedTools")) = L"mcp__kashipan_editor";
            } else {
                // Request-local overrides; never persist the endpoint or capability token to disk.
                args.insert(args.end() - 1, {L"-c", Wide("mcp_servers.kashipan_editor.url=" + Json(tools->Url()).dump()),
                    L"-c", Wide("mcp_servers.kashipan_editor.http_headers={Authorization=" + Json("Bearer " + tools->Token()).dump() + "}"),
                    L"-c", L"mcp_servers.kashipan_editor.required=true",
                    L"-c", L"mcp_servers.kashipan_editor.default_tools_approval_mode=\"approve\""});
            }
        }
        if (provider == Provider::Codex) {
            windows = WindowsSandboxArguments(windowsBackend);
            args.insert(args.begin() + 1, windows.begin(), windows.end());
        }
    } catch (...) { return false; }
    Join(); cancel_ = false;
    { std::lock_guard lock(mutex_); state_.busy = true; state_.success = false; state_.output.clear(); state_.sentence.clear(); state_.diagnostics.clear(); state_.status = "契約アカウントを再確認中…"; }
    worker_ = std::thread([this, provider, mode, account, args, windows, project, prompt, tools] {
        bool terminal = false;
        int exitCode = 1;
        try {
            auto fresh = Probe(provider, account.executable, cancel_);
            { std::lock_guard lock(mutex_); auto &stored = provider == Provider::Claude ? state_.claude : state_.codex;
                if (!fresh.subscription || fresh.executable != stored.executable) { stored.models.clear(); stored.modelsStatus.clear(); }
                stored.subscription = fresh.subscription; stored.message = fresh.message; }
            if (!fresh.subscription || cancel_) throw std::runtime_error("Subscription login unavailable");
            if (provider == Provider::Codex) {
                { std::lock_guard lock(mutex_); state_.status = "Windowsサンドボックスを確認中…"; }
                auto check = windows;
                check.insert(check.end(), {L"sandbox", L"--include-managed-config", L"--permission-profile",
                    mode == Mode::EditFiles ? L":workspace" : L":read-only", L"--", L"cmd.exe", L"/d", L"/c", L"echo", L"KASHIPAN_SANDBOX_OK"});
                std::string detail;
                const int code = Run(Wide(account.executable), check, project, {}, cancel_, 45, [&](const auto &line) {
                    if (detail.size() < 16384) detail += line.substr(0, 16384 - detail.size()) + "\n";
                });
                if (code != 0 || detail.find("KASHIPAN_SANDBOX_OK") == std::string::npos || cancel_) {
                    std::lock_guard lock(mutex_);
                    state_.diagnostics = "Windowsサンドボックスを起動できません。接続設定の方式、公式Codexのセットアップ、管理者ポリシーを確認してください。\n" + detail;
                    throw std::runtime_error("Windows sandbox unavailable");
                }
            }
            { std::lock_guard lock(mutex_); state_.status = "実行中（このCLIの契約利用枠を消費します）…"; }
            exitCode = Run(Wide(account.executable), args, project, prompt, cancel_, 1200, [&](const auto &line) {
                auto safe = line;
                if (tools) {
                    size_t pos = 0;
                    while ((pos = safe.find(tools->Token(), pos)) != std::string::npos) { safe.replace(pos, tools->Token().size(), "[redacted]"); pos += 10; }
                }
                std::lock_guard lock(mutex_); ConsumeEvent(provider, safe, state_, terminal);
                if (tools && !safe.empty() && !Json::accept(safe) && state_.diagnostics.size() < 16384)
                    state_.diagnostics += safe.substr(0, 16384 - state_.diagnostics.size()) + "\n";
            });
        } catch (...) { }
        if (tools) tools->Revoke();
        std::lock_guard lock(mutex_);
        state_.busy = false; state_.success = state_.success && terminal && exitCode == 0 && !cancel_;
        state_.status = std::string(provider == Provider::Claude ? "Claude: " : "Codex: ") + (state_.success ? "応答が完了しました。変更内容と検証結果を確認してください。" : cancel_ ?
            (mode == Mode::EditScene ? "中止しました。適用済みのシーン編集はUndoで戻せます。" : "中止しました。既に編集されたファイルは残ります。") :
            "実行できませんでした。ログイン・利用上限・モデル・CLIのバージョンを確認してください。API課金への切替は行いません。");
        if (!state_.success && !cancel_ && !state_.diagnostics.empty()) state_.status = "実行に失敗しました。「実行エラーの詳細」を確認してください。";
        if (state_.success && mode == Mode::Suggest) {
            try {
                const auto j = Json::parse(state_.output);
                auto sentence = j.value("sentence", std::string{});
                if (!sentence.empty() && sentence.size() < 768 && sentence.find_first_of("\r\n") == std::string::npos) state_.sentence = sentence;
            } catch (...) { state_.status = "回答は得られましたが、短文JSONではありません。回答を確認してください。"; }
        }
    });
    return true;
}
bool Session::Login(Provider provider, const std::string &configured, std::string &error) {
    if (Get().busy) { error = "実行を終了してからログインしてください。"; return false; }
    try {
        auto path = Resolve(provider, configured);
        if (path.empty()) { error = "公式CLIの.exeが見つかりません。"; return false; }
        std::wstring command = QuoteArgument(path.wstring()) + (provider == Provider::Claude ? L" auth login" : L" login");
        auto environment = Environment();
        STARTUPINFOW startup{}; startup.cb = sizeof(startup);
        PROCESS_INFORMATION info{};
        // Visible console is intentional: the user completes the official interactive login.
        if (!CreateProcessW(path.c_str(), command.data(), nullptr, nullptr, FALSE,
            CREATE_NEW_CONSOLE | CREATE_UNICODE_ENVIRONMENT, environment.data(), nullptr, &startup, &info)) {
            error = "公式ログイン画面を起動できませんでした。"; return false;
        }
        CloseHandle(info.hThread); CloseHandle(info.hProcess);
        error = "公式CLIでログインした後、「ログイン状態を更新」を押してください。";
        return true;
    } catch (...) { error = "CLIのパスを確認してください。"; return false; }
}
}
