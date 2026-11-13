#pragma once
#include "Debug/Logger.h"
#include <chrono>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace KashipanEngine {
/// 起動後も含め、実際のシーン切り替えに要する大まかな実時間の区間計測。
/// メインスレッド専用。親区間の合計には子区間とログ出力の負荷を含む。
class SceneChangeTiming final {
    using Clock = std::chrono::steady_clock;
public:
    explicit SceneChangeTiming(std::string prefix, std::string sceneName = {})
        : prefix_(std::move(prefix)), sceneName_(std::move(sceneName)), start_(Clock::now()), last_(start_) {}
    void End(std::string_view section) {
        Record(section, last_);
        last_ = Clock::now(); // このログ呼び出しの時間を次の区間に含めない。
    }
    void Finish() { Record("Total", start_); }
private:
    void Record(std::string_view section, Clock::time_point start) const {
        const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        auto message = std::format("[SceneChangeTiming] {}{}: {:.3f} ms ({:.3f} s)", prefix_, section, ms, ms / 1000.0);
        if (!sceneName_.empty()) message += " (scene=" + sceneName_ + ")";
        Log(message, LogSeverity::Info);
    }
    std::string prefix_;
    std::string sceneName_;
    Clock::time_point start_;
    Clock::time_point last_;
};

/// 入れ子のスコープの集計。SceneManagerがシーン切り替えを確定する間のみ有効。
/// 親と子のスコープには重複する時間があるため、合算しない。
class SceneLoadTiming final {
    using Clock = std::chrono::steady_clock;
    struct Sample {
        size_t count = 0;
        double totalMs = 0, maxMs = 0;
        std::string slowestContext;
    };
public:
    class Scope;
    class Session final {
    public:
        explicit Session(std::string sceneName) : sceneName_(std::move(sceneName)), previous_(active_) { active_ = this; }
        Session(const Session &) = delete;
        Session &operator=(const Session &) = delete;
        ~Session() noexcept {
            active_ = previous_;
            try {
                std::vector<std::pair<std::string, Sample>> ordered(samples_.begin(), samples_.end());
                std::sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) {
                    if (a.second.totalMs != b.second.totalMs) return a.second.totalMs > b.second.totalMs;
                    return a.first < b.first;
                });
                for (const auto &[key, sample] : ordered) {
                    auto message = std::format("[SceneLoadTiming] {}: total={:.3f} ms, count={}, max={:.3f} ms (scene={})",
                        key, sample.totalMs, sample.count, sample.maxMs, sceneName_);
                    if (!sample.slowestContext.empty()) message += " (slowest=" + sample.slowestContext + ")";
                    Log(message, LogSeverity::Info);
                }
            } catch (...) {} // 診断処理によってシーンの読み込みや例外処理中の巻き戻しを中断しない。
        }
    private:
        friend class Scope;
        void Add(const std::string &key, double ms, const std::string &context) {
            auto &sample = samples_[key];
            ++sample.count; sample.totalMs += ms;
            if (ms >= sample.maxMs) { sample.maxMs = ms; sample.slowestContext = context; }
        }
        std::string sceneName_;
        Session *previous_;
        std::unordered_map<std::string, Sample> samples_;
    };
    class Scope final {
    public:
        explicit Scope(std::string key, std::string context = {})
            : session_(active_), key_(std::move(key)), context_(std::move(context)),
              start_(session_ ? Clock::now() : Clock::time_point{}) {}
        Scope(const Scope &) = delete;
        Scope &operator=(const Scope &) = delete;
        ~Scope() noexcept {
            if (!session_) return;
            const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start_).count();
            try { session_->Add(key_, ms, context_); } catch (...) {}
        }
    private:
        Session *session_;
        std::string key_, context_;
        Clock::time_point start_;
    };
private:
    inline static thread_local Session *active_ = nullptr;
};
} // namespace KashipanEngine
