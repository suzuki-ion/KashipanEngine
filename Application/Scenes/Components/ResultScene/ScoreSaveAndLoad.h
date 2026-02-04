#pragma once
#include <KashipanEngine.h>
#include <vector>

namespace KashipanEngine {

class ScoreSaveAndLoad final : public ISceneComponent {
public:
    ScoreSaveAndLoad() : ISceneComponent("ScoreSaveAndLoad", 1) {}

    void RegisterScore(float score);
    void Save() const;
    void Load();

    const std::vector<float> &GetScores() const;
};

} // namespace KashipanEngine
