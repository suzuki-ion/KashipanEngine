#pragma once
#include <KashipanEngine.h>
#include <array>
#include <vector>

namespace KashipanEngine {

class Model;

class ShowScoreNumModels final : public ISceneComponent {
public:
    ShowScoreNumModels();

    void Initialize() override;
    void Update() override;

    void SetScores(const std::vector<int> &scores);
    void SetCenterPosition(size_t setIndex, const Vector3 &center);
    void SetVisible(bool visible);

private:
    static constexpr size_t kDigitCount = 6;
    static constexpr size_t kDigitVariants = 10;
    static constexpr size_t kSetCount = 4;
    static constexpr size_t kRankCount = 3;

    std::array<std::array<std::array<Model *, kDigitVariants>, kDigitCount>, kSetCount> digitModels_{};
    std::array<std::array<Vector3, kDigitCount>, kSetCount> digitPositions_{};

    std::array<Vector3, kSetCount> centerPositions_{};
    Vector3 digitScale_ = Vector3{ 1.0f, 1.0f, 1.0f };
    float digitSpacing_ = 1.0f;

    Model *rankingTextModel_ = nullptr;
    std::array<Model *, kRankCount> rankDigitModels_{};
    Vector3 rankingTextPosition_{};
    std::array<Vector3, kRankCount> rankDigitPositions_{};

    std::array<int, kSetCount> displayScores_{};
    std::vector<int> scores_{};

    bool isInitialized_ = false;
    bool positionsDirty_ = true;
    bool isVisible_ = true;

    void UpdateDigitPositions();
    void UpdateDigitModels();
    void UpdateDisplayScores();
};

} // namespace KashipanEngine
