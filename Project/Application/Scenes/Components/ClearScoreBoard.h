#pragma once

#include <KashipanEngine.h>
#include "Utilities/FileIO/JSON.h"

#include <algorithm>
#include <vector>

namespace KashipanEngine {

class ClearScoreBoard final : public ISceneComponent {
public:
    ClearScoreBoard()
        : ISceneComponent("ClearScoreBoard", 1) {}

    ~ClearScoreBoard() override = default;

    void Initialize() override {
        if (isLoaded_) return;

        const JSON root = LoadJSON(kScoreFilePath_);
        if (root.is_object() && root.contains("scores") && root["scores"].is_array()) {
            scores_.clear();
            for (const auto &v : root["scores"]) {
                if (!v.is_number_integer()) continue;
                scores_.push_back(std::max(0, v.get<int>()));
            }
            std::sort(scores_.begin(), scores_.end(), std::greater<int>());
            if (scores_.size() > kMaxStoredScores_) {
                scores_.resize(kMaxStoredScores_);
            }
        }

        isLoaded_ = true;
    }

    void AddScore(int score) {
        const int clamped = std::max(0, score);
        scores_.push_back(clamped);
        std::sort(scores_.begin(), scores_.end(), std::greater<int>());
        if (scores_.size() > kMaxStoredScores_) {
            scores_.resize(kMaxStoredScores_);
        }
        SaveScores();
    }

    std::vector<int> GetTopScores(size_t count) const {
        const size_t n = std::min(count, scores_.size());
        return std::vector<int>(scores_.begin(), scores_.begin() + static_cast<std::ptrdiff_t>(n));
    }

    int FindBestRank(int score) const {
        if (score <= 0) return -1;
        for (size_t i = 0; i < scores_.size(); ++i) {
            if (scores_[i] == score) {
                return static_cast<int>(i) + 1;
            }
        }
        return -1;
    }

private:
    void SaveScores() const {
        JSON root = JSON::object();
        root["scores"] = scores_;
        (void)SaveJSON(root, kScoreFilePath_);
    }

private:
    inline static const std::string kScoreFilePath_ = "Assets/Application/ClearScoreBoard.json";
    static constexpr size_t kMaxStoredScores_ = 100;
    inline static bool isLoaded_ = false;
    inline static std::vector<int> scores_{};
};

} // namespace KashipanEngine
