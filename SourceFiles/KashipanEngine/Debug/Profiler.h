#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace KashipanEngine {

class GameEngine;

/// 入れ子のスコープと待機を含むCPU経過時間。完了したフレームのみ公開する。
/// 固定の名前を使用する。プロセスの存続中に登録できる異なる名前は最大256個。
class Profiler final {
    static constexpr std::size_t kMaxRegisteredNames = 256;

public:
    static constexpr std::size_t kMinSampleCount = 1;
    static constexpr std::size_t kDefaultSampleCount = 60;
    static constexpr std::size_t kMaxSampleCount = 600;

    struct Sample { double milliseconds = 0; std::uint64_t calls = 0; };
    struct Result {
        std::string name;
        double milliseconds = 0, averageMs = 0, peakMs = 0;
        std::uint64_t calls = 0;
    };
    struct Snapshot {
        double frameMs = 0, fps = 0;
        std::size_t frames = 0;
        std::size_t sampleCount = kDefaultSampleCount;
        std::vector<Result> results;
        std::uint64_t frameId = 0;
    };
    static Profiler &GetInstance() { static Profiler instance; return instance; }

    // BeginFrame(), EndFrame(), SetSampleCount() はどこからでも呼び出されると困るため GameEngine だけに制限

    void BeginFrame(Passkey<GameEngine>) {
        std::lock_guard lock(mutex_);
        ++generation_;
        pending_.clear();
        recording_ = true;
        frameStart_ = Clock::now();
    }
    void EndFrame(Passkey<GameEngine>) {
        std::lock_guard lock(mutex_);
        if (!recording_) return;
        recording_ = false;
        ++completedFrames_;
        const auto end = Clock::now();
        frameMs_ = std::chrono::duration<double, std::milli>(end - frameStart_).count();
        const auto intervalStart = lastFrameEnd_ == Clock::time_point{} ? frameStart_ : lastFrameEnd_;
        const double interval = std::chrono::duration<double>(end - intervalStart).count();
        fps_ = interval > 0 ? 1.0 / interval : 0;
        lastFrameEnd_ = end;
        history_.push_back(std::make_shared<const std::map<std::string, Sample>>(std::move(pending_)));
        while (history_.size() > capacity_) history_.pop_front();
    }
    void SetSampleCount(Passkey<GameEngine>, std::size_t count) {
        std::lock_guard lock(mutex_);
        capacity_ = std::clamp<std::size_t>(count, kMinSampleCount, kMaxSampleCount);
        while (history_.size() > capacity_) history_.pop_front();
    }

    // 以下公開APIやスコープは、今後計測する箇所が増えても対応できるように常に公開

    Snapshot GetSnapshot() const {
        // ロック中は各サンプルのマップ全体ではなく、不変なフレームハンドルのみをコピーする。
        // 集計はロック外で行い、外部ビューアーによって記録が妨げられないようにする。
        Snapshot snapshot;
        std::deque<std::shared_ptr<const std::map<std::string, Sample>>> history;
        {
            std::lock_guard lock(mutex_);
            snapshot = Snapshot{frameMs_, fps_, history_.size(), capacity_, {}, completedFrames_};
            history = history_;
        }
        std::map<std::string, Result> totals;
        for (const auto &frame : history) {
            for (const auto &[name, sample] : *frame) {
                auto &result = totals[name];
                result.name = name;
                result.averageMs += sample.milliseconds;
                result.peakMs = std::max(result.peakMs, sample.milliseconds);
            }
        }
        for (auto &[name, result] : totals) {
            result.averageMs /= static_cast<double>(history.size());
            if (auto it = history.back()->find(name); it != history.back()->end()) {
                result.milliseconds = it->second.milliseconds;
                result.calls = it->second.calls;
            }
            snapshot.results.push_back(std::move(result));
        }
        return snapshot;
    }
    /// RAIIスコープ。同時記録は同期し、フレーム境界をまたぐスコープは破棄する。
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
                if (profiler.names_.size() >= kMaxRegisteredNames && !profiler.names_.contains(name_)) return;
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
    // 「Profilerはね、複数存在しないし他からも生成されないし、常に孤独であり続けなきゃいけないの。」
    Profiler() = default;

    using Clock = std::chrono::steady_clock;
    mutable std::mutex mutex_;
    std::uint64_t generation_ = 0;
    std::uint64_t completedFrames_ = 0;
    bool recording_ = false;
    std::size_t capacity_ = kDefaultSampleCount;
    double frameMs_ = 0, fps_ = 0;
    Clock::time_point frameStart_;
    Clock::time_point lastFrameEnd_;
    std::map<std::string, Sample> pending_;
    std::unordered_set<std::string> names_;
    std::deque<std::shared_ptr<const std::map<std::string, Sample>>> history_;
};

} // namespace KashipanEngine
