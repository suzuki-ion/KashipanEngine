#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/PlayerMovementController.h"

#include <algorithm>

namespace KashipanEngine {

class PlayerGetCoinCounter final : public IObjectComponent3D {
public:
    PlayerGetCoinCounter() : IObjectComponent3D("PlayerGetCoinCounter", 1) {}
    ~PlayerGetCoinCounter() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<PlayerGetCoinCounter>();
    }

    void AddCoin() {
        AddCount(1);
    }

    void ApplyRespawnPenalty() {
        const int penalty = GetCount() / 2; // 現在のコイン数の半分をペナルティとして減らす
        AddCount(-penalty);
    }

    void AddCount(int delta) {
        count_ = std::clamp(count_ + delta, 0, maxCount_);
    }

    int GetCount() const { return count_; }
    int GetMaxCount() const { return maxCount_; }

    float GetSpeedMultiplier() const {
        if (maxCount_ <= 0) return minSpeedMultiplier_;
        const float t = std::clamp(static_cast<float>(count_) / static_cast<float>(maxCount_), 0.0f, 1.0f);
        return std::clamp(Lerp(minSpeedMultiplier_, maxSpeedMultiplier_, t), 0.0f, maxSpeedMultiplier_);
    }

    std::optional<bool> Update() override {
        auto *ctx = GetOwner3DContext();
        if (!ctx) return false;

        if (!movementController_) {
            movementController_ = ctx->GetComponent<PlayerMovementController>();
        }
        if (!movementController_) return true;

        if (!forwardBehavior_) {
            forwardBehavior_ = ctx->GetComponent<PlayerForwardMoveBehavior>();
            if (forwardBehavior_) {
                baseMaxForwardSpeed_ = forwardBehavior_->GetMaxForwardSpeed();
            }
        }

        if (!forwardBehavior_) return true;

        const float multiplier = GetSpeedMultiplier();
        const float maxSpeed = std::max(0.0f, baseMaxForwardSpeed_) * multiplier;
        forwardBehavior_->SetMaxForwardSpeed(maxSpeed);
        lastAppliedMultiplier_ = multiplier;
        return true;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    PlayerMovementController *movementController_ = nullptr;
    PlayerForwardMoveBehavior *forwardBehavior_ = nullptr;
    float baseMaxForwardSpeed_ = 0.0f;

    int count_ = 0;
    int maxCount_ = 10;

    float minSpeedMultiplier_ = 1.0f;
    float maxSpeedMultiplier_ = 3.0f;
    float lastAppliedMultiplier_ = 1.0f;
};

} // namespace KashipanEngine
