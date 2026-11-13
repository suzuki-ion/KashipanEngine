#pragma once

#include "Debug/Logger.h"
#include <chrono>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace KashipanEngine {

/// フレームプロファイラーから独立した、起動時の実時間計測。
/// メインスレッドからのみ呼び出す。入れ子の区間には子区間を含む。
class StartupTiming final {
    using Clock = std::chrono::steady_clock;
public:
    static void Begin() {
        start_ = Clock::now();
        pending_.clear();
        loggingReady_ = false;
        active_ = true;
    }
    static bool IsActive() { return active_; }
    static void EnableLogging() {
        loggingReady_ = true;
        for (const auto &line : pending_) Log(line, LogSeverity::Info);
        pending_.clear();
    }
    static void Milestone(const std::string &name) {
        if (active_) Record(name, start_);
    }
    static void Complete() {
        Milestone("Startup.Total (engine entry -> startup scene first frame drawn)");
        active_ = false;
    }
    static void Interrupt() {
        Milestone("Startup.Interrupted (startup did not complete)");
        active_ = false;
    }

    /// 1つの初期化処理内で連続する、重複しない区間。
    class Sections final {
    public:
        explicit Sections(std::string prefix) : prefix_(std::move(prefix)), last_(active_ ? Clock::now() : Clock::time_point{}) {}
        void End(std::string_view name) {
            if (!active_) return;
            Record(prefix_ + std::string(name), last_);
            // この計測ログのキュー投入に要する時間を次の区間から除外する。
            last_ = Clock::now();
        }
    private:
        std::string prefix_;
        Clock::time_point last_;
    };

private:
    static void Record(const std::string &name, Clock::time_point start) {
        const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        const auto line = std::format("[StartupTiming] {}: {:.3f} ms ({:.3f} s)", name, ms, ms / 1000.0);
        if (loggingReady_) Log(line, LogSeverity::Info);
        else pending_.push_back(line);
    }
    inline static Clock::time_point start_;
    inline static std::vector<std::string> pending_;
    inline static bool active_ = false;
    inline static bool loggingReady_ = false;
};

} // namespace KashipanEngine
