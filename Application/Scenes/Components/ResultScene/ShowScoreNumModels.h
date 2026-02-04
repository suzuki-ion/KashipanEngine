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

    void SetScores(const std::vector<float> &scores);
    void SetCenterPosition(size_t setIndex, const Vector3 &center);

private:
    static constexpr size_t kDigitCount = 6;
    static constexpr size_t kDigitVariants = 10;
    static constexpr size_t kSetCount = 4;

    std::array<std::array<std::array<Model *, kDigitVariants>, kDigitCount>, kSetCount> digitModels_{};
    std::array<std::array<Vector3, kDigitCount>, kSetCount> digitPositions_{};

    std::array<Vector3, kSetCount> centerPositions_{};
    Vector3 digitScale_ = Vector3{ 1.0f, 1.0f, 1.0f };
    float digitSpacing_ = 1.0f;

    std::array<float, kSetCount> displayScores_{};
    std::vector<float> scores_{};

    bool isInitialized_ = false;
    bool positionsDirty_ = true;

    void UpdateDigitPositions();
    void UpdateDigitModels();
    void UpdateDisplayScores();
};

} // namespace KashipanEngine
