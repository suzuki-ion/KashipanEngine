#pragma once
#include "Utilities/Passkeys.h"
#include <filesystem>
#include <string>
#include <cstdlib>

namespace KashipanEngine {

/// 現在のシーンから独立し、エディターが所有するプロセスハンドル。
/// エディター終了時にはハンドルを解放するが、単独のビューアーは動作を継続する。
class ProfilerLauncher final {
public:
    static ProfilerLauncher &GetInstance() { static ProfilerLauncher value; return value; }
    ~ProfilerLauncher() { if (process_) CloseHandle(process_); }
    ProfilerLauncher() = default;
    ProfilerLauncher(const ProfilerLauncher &) = delete;
    ProfilerLauncher &operator=(const ProfilerLauncher &) = delete;

    bool IsRunning() {
        Poll();
        return process_ != nullptr;
    }
    void Poll() {
        if (!process_ || WaitForSingleObject(process_, 0) != WAIT_OBJECT_0) return;
        DWORD code = 0;
        if (!GetExitCodeProcess(process_, &code)) code = GetLastError();
        CloseHandle(process_); process_ = nullptr;
        if (code) Fail("editor.profiler.exited", code);
    }
    bool TakeError() { const bool value = newError_; newError_ = false; return value; }
    const char *ErrorKey() const { return errorKey_; }
    DWORD ErrorCode() const { return errorCode_; }

    static unsigned short Port() {
        char value[32]{};
        const DWORD count = GetEnvironmentVariableA("KASHIPAN_PROFILER_PORT", value, sizeof(value));
        if (!count) return 47180;
        if (count >= sizeof(value)) return 0;
        char *end = nullptr;
        const auto number = std::strtoul(value, &end, 10);
        return end == value || *end || number > 65535 ? 0 : static_cast<unsigned short>(number);
    }
    static std::filesystem::path FindScript(const std::filesystem::path &engineRoot,
        const std::filesystem::path &executableDirectory) {
        for (const auto &root : {engineRoot, executableDirectory}) {
            const auto candidate = root / "EngineTools" / "Profiler" / "profiler.py";
            std::error_code error;
            if (std::filesystem::is_regular_file(candidate, error)) return candidate;
        }
        return {};
    }
    bool Start(const std::filesystem::path &engineRoot, const std::filesystem::path &executableDirectory) {
        if (IsRunning()) return true;
        const auto port = Port();
        if (!port) return Fail("editor.profiler.disabled");
        const auto script = FindScript(engineRoot, executableDirectory);
        if (script.empty()) return Fail("editor.profiler.missing_script");
        for (const auto *name : {L"pyw.exe", L"pythonw.exe", L"py.exe", L"python.exe"}) {
            wchar_t path[32768]{};
            const DWORD count = SearchPathW(nullptr, name, nullptr, 32768, path, nullptr);
            if (count && count < 32768) {
                const std::wstring executable(name);
                return StartProcess(script, path, executable == L"pyw.exe" || executable == L"py.exe", port);
            }
        }
        return Fail("editor.profiler.missing_python");
    }
    /// ネイティブプロセスの回帰テストでも使用する、解決済みのエントリーポイント。
    bool StartProcess(const std::filesystem::path &script, const std::filesystem::path &python,
        bool usePythonLauncher, unsigned short port) {
        if (IsRunning()) return true;
        std::wstring command = L"\"" + python.wstring() + L"\"" +
            (usePythonLauncher ? L" -3" : L"") + L" \"" + script.wstring() + L"\" --port " + std::to_wstring(port);
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION info{};
        // シェル展開・エンジンのハンドル継承・コンソールウィンドウを使用しない。
        if (!CreateProcessW(python.c_str(), command.data(), nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, script.parent_path().c_str(), &startup, &info))
            return Fail("editor.profiler.launch_failed", GetLastError());
        CloseHandle(info.hThread);
        process_ = info.hProcess;
        newError_ = false;
        return true;
    }
private:
    bool Fail(const char *key, DWORD code = 0) {
        errorKey_ = key; errorCode_ = code; newError_ = true;
        return false;
    }
    HANDLE process_ = nullptr;
    const char *errorKey_ = "editor.profiler.launch_failed";
    DWORD errorCode_ = 0;
    bool newError_ = false;
};
} // namespace KashipanEngine
