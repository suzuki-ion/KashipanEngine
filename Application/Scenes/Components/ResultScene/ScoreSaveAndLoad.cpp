#include "ScoreSaveAndLoad.h"
#include "Utilities/FileIO/Json.h"
#include <fstream>

namespace KashipanEngine {

namespace {
constexpr const char *kScoreDataFile = "ScoreData.json";
std::vector<int> sScores = { 0, 0, 0 };
} // namespace

void ScoreSaveAndLoad::RegisterScore(int score) {
    sScores.push_back(score);
}

void ScoreSaveAndLoad::Save() const {
    Json jsonData;
    jsonData["scores"] = sScores;
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
            sScores = jsonData["scores"].get<std::vector<int>>();
        } catch (const std::exception &) {
        }
    }
}

const std::vector<int> &ScoreSaveAndLoad::GetScores() const {
    return sScores;
}

} // namespace KashipanEngine
