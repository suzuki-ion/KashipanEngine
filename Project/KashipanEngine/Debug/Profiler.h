#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace KashipanEngine {

/// CPU elapsed time, inclusive of nested scopes and waits. Only completed frames are published.
/// Use stable names: at most 256 distinct names are accepted during the process lifetime.
class Profiler final {
public:
    struct Sample { double milliseconds = 0; std::uint64_t calls = 0; };
    struct Result {
        std::string name;
        double milliseconds = 0, averageMs = 0, peakMs = 0;
        std::uint64_t calls = 0;
    };
    struct Snapshot {
        double frameMs = 0, fps = 0;
        std::size_t frames = 0;
        std::size_t sampleCount = 60;
        std::vector<Result> results;
    };
    static Profiler &GetInstance() { static Profiler instance; return instance; }
    void BeginFrame() {
        std::lock_guard lock(mutex_);
        ++generation_;
        pending_.clear();
        recording_ = true;
        frameStart_ = Clock::now();
    }
    void EndFrame() {
        std::lock_guard lock(mutex_);
        if (!recording_) return;
        recording_ = false;
        const auto end = Clock::now();
        frameMs_ = std::chrono::duration<double, std::milli>(end - frameStart_).count();
        const auto intervalStart = lastFrameEnd_ == Clock::time_point{} ? frameStart_ : lastFrameEnd_;
        const double interval = std::chrono::duration<double>(end - intervalStart).count();
        fps_ = interval > 0 ? 1.0 / interval : 0;
        lastFrameEnd_ = end;
        history_.push_back(std::move(pending_));
        while (history_.size() > capacity_) history_.pop_front();
    }
    void SetSampleCount(std::size_t count) {
        std::lock_guard lock(mutex_);
        capacity_ = std::clamp<std::size_t>(count, 1, 600);
        while (history_.size() > capacity_) history_.pop_front();
    }
    Snapshot GetSnapshot() const {
        std::lock_guard lock(mutex_);
        Snapshot snapshot{frameMs_, fps_, history_.size(), capacity_, {}};
        std::map<std::string, Result> totals;
        for (const auto &frame : history_) {
            for (const auto &[name, sample] : frame) {
                auto &result = totals[name];
                result.name = name;
                result.averageMs += sample.milliseconds;
                result.peakMs = std::max(result.peakMs, sample.milliseconds);
            }
        }
        for (auto &[name, result] : totals) {
            result.averageMs /= static_cast<double>(history_.size());
            if (auto it = history_.back().find(name); it != history_.back().end()) {
                result.milliseconds = it->second.milliseconds;
                result.calls = it->second.calls;
            }
            snapshot.results.push_back(std::move(result));
        }
        return snapshot;
    }
    /// RAII scope. Concurrent records are synchronized; scopes crossing a frame boundary are discarded.
    class Scope final {
    public:
        explicit Scope(std::string name) : name_(std::move(name)) {
            auto &profiler = GetInstance();
            std::lock_guard lock(profiler.mutex_);
            generation_ = profiler.recording_ ? profiler.generation_ : 0;
            start_ = Clock::now();
        }
        ~Scope() {
            if (!generation_) return;
            const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start_).count();
            auto &profiler = GetInstance();
            std::lock_guard lock(profiler.mutex_);
            if (!profiler.recording_ || generation_ != profiler.generation_) return;
            if (!profiler.pending_.contains(name_)) {
                if (profiler.names_.size() >= 256 && !profiler.names_.contains(name_)) return;
                profiler.names_.insert(name_);
            }
            auto &sample = profiler.pending_[name_];
            sample.milliseconds += ms;
            ++sample.calls;
        }
        Scope(const Scope &) = delete;
        Scope &operator=(const Scope &) = delete;
    private:
        std::string name_;
        std::uint64_t generation_ = 0;
        std::chrono::steady_clock::time_point start_;
    };
private:
    using Clock = std::chrono::steady_clock;
    mutable std::mutex mutex_;
    std::uint64_t generation_ = 0;
    bool recording_ = false;
    std::size_t capacity_ = 60;
    double frameMs_ = 0, fps_ = 0;
    Clock::time_point frameStart_;
    Clock::time_point lastFrameEnd_;
    std::map<std::string, Sample> pending_;
    std::unordered_set<std::string> names_;
    std::deque<std::map<std::string, Sample>> history_;
};

} // namespace KashipanEngine
