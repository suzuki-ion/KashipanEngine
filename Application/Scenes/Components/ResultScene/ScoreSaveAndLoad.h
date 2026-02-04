#pragma once
#include <KashipanEngine.h>
#include <vector>

namespace KashipanEngine {

class ScoreSaveAndLoad final : public ISceneComponent {
public:
    ScoreSaveAndLoad() : ISceneComponent("ScoreSaveAndLoad", 1) {}

    void RegisterScore(int score);
    void Save() const;
    void Load();

    const std::vector<int> &GetScores() const;
};

} // namespace KashipanEngine
