#pragma once
#include <KashipanEngine.h>
#include <array>
#include "BackMonitorRenderer.h"
#include "Objects/Components/Player/ScoreManager.h"

namespace KashipanEngine {

class Model;

class BackMonitorWithScoreScreen : public BackMonitorRenderer {
public:
    BackMonitorWithScoreScreen(ScreenBuffer *target, ScoreManager *scoreManager);
    ~BackMonitorWithScoreScreen() override;

    void Initialize() override;
    void Update() override;

    int GetScore() const { return static_cast<int>(score_); }
    int GetDisplayScore() const { return static_cast<int>(displayScore_); }

private:
    static constexpr size_t kDigitCount = 6;
    static constexpr size_t kDigitVariants = 10;

    ScoreManager *scoreManager_ = nullptr;

    std::array<std::array<Model *, kDigitVariants>, kDigitCount> digitModels_{};
    std::array<Vector3, kDigitCount> digitPositions_{};

    Vector3 centerPosition_ = Vector3{ 0.0f, -1.0f, 2.0f };
    Vector3 digitScale_ = Vector3{ 1.0f, 1.0f, 1.0f };
    float digitSpacing_ = 1.0f;

    float score_ = 0.0f;
    float displayScore_ = 0.0f;

    int lastScore_ = -1;
    float scaleAnimElapsed_ = 0.0f;
    bool scaleAnimating_ = false;
    const float scaleAnimDuration_ = 0.5f;
    const float scaleAnimStartY_ = 2.0f;

    bool isInitialized_ = false;
    bool wasActive_ = false;
};

} // namespace KashipanEngine
