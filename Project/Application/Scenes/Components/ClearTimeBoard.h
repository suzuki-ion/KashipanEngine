#pragma once

#include <KashipanEngine.h>
#include "Utilities/FileIO/JSON.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace KashipanEngine {

class ClearTimeBoard final : public ISceneComponent {
public:
    using TimeValue = std::uint64_t;

    ClearTimeBoard()
        : ISceneComponent("ClearTimeBoard", 1) {}

    ~ClearTimeBoard() override = default;

    void Initialize() override {
        if (isInitialized_) return;

        currentStageFilePath_ = GetOwnerContext()
            ? GetOwnerContext()->GetSceneVariableOr<std::string>(kStageFilePathVariableName_, std::string{})
            : std::string{};

        LoadFromJsonFile();
        elapsedMilliseconds_ = 0;
        isMeasuring_ = false;
        isInitialized_ = true;
    }

    void Update() override {
        if (!isMeasuring_) return;

        const float dt = std::max(0.0f, GetDeltaTime());
        if (dt <= 0.0f) return;

        elapsedMilliseconds_ += static_cast<TimeValue>(dt * 1000.0f);
    }

    void StartMeasurement() {
        isMeasuring_ = true;
    }

    void PauseMeasurement() {
        isMeasuring_ = false;
    }

    void ResetMeasurement() {
        elapsedMilliseconds_ = 0;
    }

    TimeValue RegisterCurrentTime() {
        const TimeValue current = GetCurrentTimeMilliseconds();
        auto &times = stageTimeHistory_[GetStageKey()];
        times.push_back(current);
        std::sort(times.begin(), times.end());
        if (times.size() > kMaxStoredTimes_) {
            times.resize(kMaxStoredTimes_);
        }
        SaveToJsonFile();
        return current;
    }

    void LoadFromJsonFile(const std::string &filePath = kDefaultFilePath_) {
        stageTimeHistory_.clear();

        const JSON root = LoadJSON(filePath);
        const JSON *stagesNode = &root;
        if (root.is_object() && root.contains("stages") && root["stages"].is_object()) {
            stagesNode = &root["stages"];
        }

        if (stagesNode->is_object()) {
            for (auto it = stagesNode->begin(); it != stagesNode->end(); ++it) {
                if (!it.value().is_array()) continue;

                std::vector<TimeValue> times;
                for (const auto &v : it.value()) {
                    if (v.is_number_unsigned()) {
                        times.push_back(v.get<TimeValue>());
                    } else if (v.is_number_integer()) {
                        times.push_back(static_cast<TimeValue>(std::max(0, v.get<int>())));
                    } else if (v.is_number_float()) {
                        const double seconds = std::max(0.0, v.get<double>());
                        times.push_back(static_cast<TimeValue>(seconds * 1000.0));
                    }
                }

                std::sort(times.begin(), times.end());
                if (times.size() > kMaxStoredTimes_) {
                    times.resize(kMaxStoredTimes_);
                }
                stageTimeHistory_[it.key()] = std::move(times);
            }
        }

    }

    void SaveToJsonFile(const std::string &filePath = kDefaultFilePath_) const {
        JSON root = JSON::object();
        JSON stages = JSON::object();

        for (const auto &entry : stageTimeHistory_) {
            JSON times = JSON::array();
            for (const auto timeValue : entry.second) {
                times.push_back(timeValue);
            }
            stages[entry.first] = std::move(times);
        }

        root["stages"] = std::move(stages);
        (void)SaveJSON(root, filePath);
    }

    TimeValue GetCurrentTimeMilliseconds() const {
        return elapsedMilliseconds_;
    }

    double GetCurrentTimeSeconds() const {
        return static_cast<double>(elapsedMilliseconds_) / 1000.0;
    }

    std::vector<TimeValue> GetTopTimes(size_t count) const {
        const auto &times = GetCurrentStageTimes();
        const size_t n = std::min(count, times.size());
        return std::vector<TimeValue>(times.begin(), times.begin() + static_cast<std::ptrdiff_t>(n));
    }

    int FindBestRank(TimeValue timeValue) const {
        if (timeValue == 0) return -1;

        const auto &times = GetCurrentStageTimes();
        for (size_t i = 0; i < times.size(); ++i) {
            if (times[i] == timeValue) {
                return static_cast<int>(i) + 1;
            }
        }
        return -1;
    }

    const std::string &GetCurrentStageFilePath() const {
        return currentStageFilePath_;
    }

private:
    const std::vector<TimeValue> &GetCurrentStageTimes() const {
        static const std::vector<TimeValue> kEmptyTimes{};
        const auto it = stageTimeHistory_.find(GetStageKey());
        return it != stageTimeHistory_.end() ? it->second : kEmptyTimes;
    }

    const std::string &GetStageKey() const {
        return currentStageFilePath_;
    }

private:
    inline static const std::string kDefaultFilePath_ = "Assets/Application/ClearTimeBoard.json";
    inline static const std::string kStageFilePathVariableName_ = "TargetStageFilePath";
    static constexpr size_t kMaxStoredTimes_ = 100;

    bool isInitialized_ = false;
    bool isMeasuring_ = false;
    TimeValue elapsedMilliseconds_ = 0;
    std::string currentStageFilePath_;
    std::unordered_map<std::string, std::vector<TimeValue>> stageTimeHistory_{};
};

} // namespace KashipanEngine