#pragma once

#include "Debug/Logger.h"
#include <chrono>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace KashipanEngine {

/// Startup wall-clock measurements, independent of the frame profiler.
/// Called only on the main thread. Nested sections include their child sections.
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

    /// Consecutive non-overlapping sections within one initialization routine.
    class Sections final {
    public:
        explicit Sections(std::string prefix) : prefix_(std::move(prefix)), last_(active_ ? Clock::now() : Clock::time_point{}) {}
        void End(std::string_view name) {
            if (!active_) return;
            Record(prefix_ + std::string(name), last_);
            // Exclude this timing log's enqueue overhead from the next section.
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
