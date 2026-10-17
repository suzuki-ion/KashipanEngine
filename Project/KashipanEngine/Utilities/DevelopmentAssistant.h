#pragma once
#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <memory>
#include <vector>

namespace KashipanEngine::DevelopmentAI {
enum class Provider { Claude, Codex };
enum class Mode { Suggest, EditFiles, EditScene };
class EditorTools;
struct Model { std::string id, name, description; };
struct Account {
    std::string executable;
    bool installed = false;
    bool subscription = false;
    std::string message;
    std::vector<Model> models;
    std::string modelsStatus;
};
struct Snapshot {
    bool busy = false;
    bool success = false;
    Account claude, codex;
    std::string status, output, sentence, diagnostics;
};
// These functions are also used by the offline regression tests.
bool IsSubscription(Provider provider, const std::string &status, int exitCode);
std::wstring QuoteArgument(const std::wstring &argument);
bool IsBillingEnvironment(const std::wstring &name);
std::vector<std::wstring> RequestArguments(Provider provider, Mode mode, const std::string &model);
void ConsumeEvent(Provider provider, const std::string &line, Snapshot &state, bool &terminal);
std::vector<Model> ParseModels(Provider provider, const std::string &json);
std::vector<std::wstring> WindowsSandboxArguments(const std::string &backend);

// Only the worker owns process handles. No scene pointers or credentials are retained.
class Session final {
public:
    ~Session();
    Snapshot Get() const;
    void Refresh(const std::string &claudePath, const std::string &codexPath);
    void RefreshModels(Provider provider, const std::string &path);
    bool Start(Provider provider, Mode mode, const std::string &model,
        const std::filesystem::path &project, const std::string &prompt, const std::string &windowsBackend = "auto",
        std::shared_ptr<EditorTools> tools = {});
    bool Login(Provider provider, const std::string &path, std::string &error);
    void Cancel();
private:
    void Join();
    mutable std::mutex mutex_;
    Snapshot state_;
    std::thread worker_;
    std::atomic<bool> cancel_{false};
};
}
