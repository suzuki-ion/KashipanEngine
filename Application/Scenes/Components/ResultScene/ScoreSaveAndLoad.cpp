#include "ScoreSaveAndLoad.h"
#include "Utilities/FileIO/Json.h"
#include <fstream>

namespace KashipanEngine {

namespace {
constexpr const char *kScoreDataFile = "ScoreData.json";
std::vector<float> g_scores;
} // namespace

void ScoreSaveAndLoad::RegisterScore(float score) {
    g_scores.push_back(score);
}

void ScoreSaveAndLoad::Save() const {
    Json jsonData;
    jsonData["scores"] = g_scores;
    SaveJSON(jsonData, kScoreDataFile);
}

void ScoreSaveAndLoad::Load() {
    std::ifstream file(kScoreDataFile);
    if (!file.is_open()) {
        return;
    }

    Json jsonData = LoadJSON(kScoreDataFile);
    if (jsonData.contains("scores") && jsonData["scores"].is_array()) {
        try {
            g_scores = jsonData["scores"].get<std::vector<float>>();
            return;
        } catch (const std::exception &) {
        }
    }
    g_scores.clear();
}

const std::vector<float> &ScoreSaveAndLoad::GetScores() const {
    return g_scores;
}

} // namespace KashipanEngine
