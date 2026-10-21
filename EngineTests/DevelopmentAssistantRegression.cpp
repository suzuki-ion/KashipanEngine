#include "Utilities/DevelopmentAssistant.h"
#include "Utilities/DevelopmentAssistantTools.h"
#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <chrono>
#include <iostream>
#include <fstream>
#include <thread>
#include <json.hpp>

using namespace KashipanEngine::DevelopmentAI;
namespace {
Snapshot Wait(Session &session) {
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (session.Get().busy && std::chrono::steady_clock::now() < limit) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto state = session.Get(); assert(!state.busy); return state;
}
std::string Utf8(const std::wstring &s) {
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string value(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), value.data(), n, nullptr, nullptr); return value;
}
}
int main(int argc, char **argv) {
    // This executable is also the fake official CLI. No model or network is used.
    if (argc > 1) {
        char billing[128];
        if (GetEnvironmentVariableA("OPENAI_API_KEY", billing, sizeof(billing)) ||
            GetEnvironmentVariableA("ANTHROPIC_API_KEY", billing, sizeof(billing))) return 9;
        if (std::string(argv[1]) == "auth") {
            std::cout << "{\"loggedIn\":true,\"authMethod\":\"claude.ai\",\"apiProvider\":\"firstParty\"}\n"; return 0;
        }
        if (std::string(argv[1]) == "login") { std::cerr << "Logged in using ChatGPT\n"; return 0; }
        if (std::string(argv[1]) == "--sleep") { Sleep(30000); return 0; }
        for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "sandbox") {
            char fail[32];
            if (GetEnvironmentVariableA("KASHIPAN_FAKE_SANDBOX_FAIL", fail, sizeof(fail))) {
                std::cerr << "setup refresh had errors (offline fixture)\n"; return 7;
            }
            std::cout << "KASHIPAN_SANDBOX_OK\n"; return 0;
        }
        bool catalog = std::string(argv[1]) == "app-server";
        for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--input-format") catalog = true;
        if (catalog) {
            std::string line;
            while (std::getline(std::cin, line)) {
                auto request = nlohmann::json::parse(line);
                if (std::string(argv[1]) != "app-server") {
                    assert(request.at("request").at("subtype") == "initialize");
                    std::cout << R"({"type":"control_response","response":{"request_id":"catalog","subtype":"success","response":{"models":[{"value":"default","displayName":"Default"},{"value":"sonnet","displayName":"Sonnet","description":"Example"}]}}})" << '\n' << std::flush;
                } else if (request.value("method", std::string{}) == "initialize") {
                    std::cout << R"({"id":1,"result":{}})" << '\n' << std::flush;
                } else if (request.value("method", std::string{}) == "model/list") {
                    if (!request.at("params").contains("cursor"))
                        std::cout << R"({"id":2,"result":{"data":[{"model":"model-a","displayName":"A"},{"model":"hidden-model","hidden":true}],"nextCursor":"page2"}})" << '\n' << std::flush;
                    else
                        std::cout << R"({"id":2,"result":{"data":[{"model":"model-a"},{"model":"model-b","displayName":"B"}],"nextCursor":null}})" << '\n' << std::flush;
                }
            }
            return 0;
        }
        std::string input((std::istreambuf_iterator<char>(std::cin)), {});
        if (input == "SCENE") {
            bool configured = false;
            for (int i = 1; i < argc; ++i) {
                const std::string arg = argv[i];
                if (arg == "workspace-write") return 8;
                if (arg == "--mcp-config" && i + 1 < argc) {
                    const auto config = nlohmann::json::parse(argv[i + 1]);
                    const auto &server = config.at("mcpServers").at("kashipan_editor");
                    configured = server.at("type") == "http" && server.at("url").get<std::string>().starts_with("http://127.0.0.1:") &&
                        server.at("headers").at("Authorization").get<std::string>().starts_with("Bearer ");
                }
                if (arg.starts_with("mcp_servers.kashipan_editor.http_headers={Authorization=\"Bearer ")) configured = true;
            }
            if (!configured) return 9;
            if (std::string(argv[1]) == "exec") std::cout << "{\"type\":\"turn.completed\"}\n";
            else std::cout << "{\"type\":\"result\",\"subtype\":\"success\",\"is_error\":false,\"result\":\"scene response\"}\n";
            return 0;
        }
        if (input == "WAIT") { Sleep(30000); return 0; }
        if (input == "CHILD") {
            wchar_t self[32768]; GetModuleFileNameW(nullptr, self, 32768);
            auto command = QuoteArgument(self) + L" --sleep";
            STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION process{};
            if (!CreateProcessW(self, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) return 8;
            std::cout << "{\"type\":\"item.completed\",\"item\":{\"type\":\"agent_message\",\"text\":\"child=" << process.dwProcessId << "\"}}\n" << std::flush;
            CloseHandle(process.hThread); CloseHandle(process.hProcess); Sleep(30000); return 0;
        }
        if (input == "EDIT") {
            bool editable = false;
            for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "workspace-write") editable = true;
            if (!editable) return 7;
            std::ofstream("assistant-edit-test.txt") << "edited in requested cwd";
            std::cout << "{\"type\":\"turn.completed\"}\n"; return 0;
        }
        if (input == "FAIL") { std::cout << "{\"type\":\"turn.failed\",\"error\":{\"message\":\"quota\"}}\n"; return 0; }
        if (input == "INCOMPLETE") { std::cout << "{\"type\":\"item.completed\",\"item\":{\"type\":\"agent_message\",\"text\":\"partial\"}}\n"; return 0; }
        assert(input == "日本語 & $(do not execute) \"quoted\"\n");
        if (std::string(argv[1]) == "exec") {
            std::cout << "{\"type\":\"item.completed\",\"item\":{\"type\":\"agent_message\",\"text\":\"{\\\"sentence\\\":\\\"Transformの移動X軸を2にする\\\"}\"}}\n";
            std::cout << "{\"type\":\"turn.completed\"}\n";
        } else {
            std::cout << "{\"type\":\"result\",\"subtype\":\"success\",\"is_error\":false,\"result\":\"{\\\"sentence\\\":\\\"Transformの移動X軸を2にする\\\"}\"}\n";
        }
        return 0;
    }
    assert(IsSubscription(Provider::Codex, "Logged in using ChatGPT\n", 0));
    assert(!IsSubscription(Provider::Codex, "Logged in using API key", 0));
    assert(!IsSubscription(Provider::Codex, "Logged in using ChatGPT", 1));
    assert(IsSubscription(Provider::Claude, R"({"loggedIn":true,"authMethod":"claude.ai","apiProvider":"firstParty"})", 0));
    assert(!IsSubscription(Provider::Claude, R"({"loggedIn":true,"authMethod":"api_key","apiProvider":"firstParty"})", 0));
    assert(!IsSubscription(Provider::Claude, "not JSON", 0));
    assert(IsBillingEnvironment(L"anthropic_api_key"));
    assert(IsBillingEnvironment(L"OPENAI_BASE_URL"));
    assert(IsBillingEnvironment(L"CLAUDE_CODE_USE_BEDROCK"));
    assert(!IsBillingEnvironment(L"PATH"));
    assert(!IsBillingEnvironment(L"USERPROFILE"));
    assert(IsBillingEnvironment(L"CODEX_PERMISSION_PROFILE"));
    const auto windows = WindowsSandboxArguments("mxc");
    assert(std::find(windows.begin(), windows.end(), L"windows.sandbox=\"mxc\"") != windows.end());
    assert(QuoteArgument(L"a b") == L"\"a b\"");
    assert(QuoteArgument(L"a\\") == L"\"a\\\\\"");
    assert(QuoteArgument(L"a\"b") == L"\"a\\\"b\"");
    bool rejected = false;
    try { RequestArguments(Provider::Codex, Mode::Suggest, "model & calc"); } catch (...) { rejected = true; }
    assert(rejected);
    Snapshot event; bool terminal = false;
    ConsumeEvent(Provider::Claude, R"({"type":"result","is_error":true,"subtype":"error_during_execution"})", event, terminal);
    assert(terminal && !event.success);
    terminal = false; ConsumeEvent(Provider::Codex, "malformed stderr", event, terminal); assert(!terminal);
    wchar_t path[32768]; GetModuleFileNameW(nullptr, path, 32768);
    const auto exe = Utf8(path);
    SetEnvironmentVariableW(L"OPENAI_API_KEY", L"offline-test-dummy");
    SetEnvironmentVariableW(L"ANTHROPIC_API_KEY", L"offline-test-dummy");
    Session session;
    session.Refresh(exe, exe);
    auto snapshot = Wait(session);
    assert(snapshot.claude.subscription && snapshot.codex.subscription);
    assert(snapshot.claude.models.size() == 1 && snapshot.claude.models[0].id == "sonnet");
    assert(snapshot.codex.models.size() == 2 && snapshot.codex.models[1].id == "model-b");
    auto models = ParseModels(Provider::Claude, R"({"models":[{"value":"default"},{"value":"safe-model"},{"value":"safe-model"},{"value":"bad model"}]})");
    assert(models.size() == 1 && models[0].id == "safe-model");
    session.RefreshModels(Provider::Codex, exe); snapshot = Wait(session);
    assert(snapshot.codex.models.size() == 2 && snapshot.claude.models.size() == 1);
    assert(!session.Start(Provider::Codex, Mode::EditScene, "", std::filesystem::current_path(), "SCENE"));
    for (auto provider : {Provider::Claude, Provider::Codex}) {
        auto tools = std::make_shared<EditorTools>("[]");
        assert(!session.Start(provider, Mode::Suggest, "", std::filesystem::current_path(), "SCENE", "auto", tools));
        assert(session.Start(provider, Mode::EditScene, "", std::filesystem::current_path(), "SCENE", "auto", tools));
        snapshot = Wait(session); assert(snapshot.success && snapshot.sentence.empty());
    }
    SetEnvironmentVariableW(L"KASHIPAN_FAKE_SANDBOX_FAIL", L"1");
    assert(session.Start(Provider::Codex, Mode::EditFiles, "", std::filesystem::current_path(), "UNREACHED"));
    snapshot = Wait(session); assert(!snapshot.success && snapshot.output.empty() && snapshot.diagnostics.find("setup refresh") != std::string::npos);
    SetEnvironmentVariableW(L"KASHIPAN_FAKE_SANDBOX_FAIL", nullptr);
    const std::string prompt = "日本語 & $(do not execute) \"quoted\"\n";
    for (auto provider : {Provider::Claude, Provider::Codex}) {
        assert(session.Start(provider, Mode::Suggest, "", std::filesystem::current_path(), prompt));
        snapshot = Wait(session);
        assert(snapshot.success && snapshot.sentence == "Transformの移動X軸を2にする");
    }
    assert(session.Start(Provider::Codex, Mode::Suggest, "", std::filesystem::current_path(), "FAIL"));
    snapshot = Wait(session); assert(!snapshot.success && snapshot.sentence.empty());
    assert(session.Start(Provider::Codex, Mode::Suggest, "", std::filesystem::current_path(), "INCOMPLETE"));
    snapshot = Wait(session); assert(!snapshot.success && snapshot.sentence.empty());
    assert(session.Start(Provider::Codex, Mode::Suggest, "", std::filesystem::current_path(), "WAIT"));
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); session.Cancel();
    snapshot = Wait(session); assert(!snapshot.success);
    wchar_t temporary[32768]; GetTempPathW(32768, temporary);
    auto editRoot = std::filesystem::path(temporary) / (L"DevelopmentAI-日本語 space-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(editRoot);
    assert(session.Start(Provider::Codex, Mode::EditFiles, "", editRoot, "EDIT"));
    snapshot = Wait(session); assert(snapshot.success && std::filesystem::exists(editRoot / L"assistant-edit-test.txt"));
    std::filesystem::remove(editRoot / L"assistant-edit-test.txt"); std::filesystem::remove(editRoot);
    assert(session.Start(Provider::Codex, Mode::Suggest, "", std::filesystem::current_path(), "CHILD"));
    for (int i = 0; i < 400 && session.Get().output.find("child=") == std::string::npos; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto output = session.Get().output; auto at = output.find("child="); assert(at != std::string::npos);
    auto pid = static_cast<DWORD>(std::stoul(output.substr(at + 6)));
    auto child = OpenProcess(SYNCHRONIZE, FALSE, pid); assert(child);
    session.Cancel(); snapshot = Wait(session); assert(!snapshot.success);
    assert(WaitForSingleObject(child, 3000) == WAIT_OBJECT_0); CloseHandle(child);
    session.Refresh("C:/does-not-exist.exe", exe); snapshot = Wait(session);
    assert(!snapshot.claude.installed && snapshot.codex.subscription);
    assert(!session.Start(Provider::Claude, Mode::Suggest, "", std::filesystem::current_path(), prompt));
    std::cout << "Development assistant offline regressions passed\n";
}
