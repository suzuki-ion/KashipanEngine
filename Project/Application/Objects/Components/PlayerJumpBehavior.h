#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/PlayerMovementControllerAccess.h"

#include <algorithm>

namespace KashipanEngine {

class PlayerJumpBehavior final : public IObjectComponent3D {
public:
    PlayerJumpBehavior() : IObjectComponent3D("PlayerJumpBehavior", 1) {}
    ~PlayerJumpBehavior() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<PlayerJumpBehavior>();
    }

    void RequestJump() {
        if (jumpCount_ >= maxJumpCount_) return;
        jumpRequested_ = true;
    }

    void SetJumpInputHeld(bool held) {
        jumpInputHeld_ = held;
        if (!jumpInputHeld_) {
            jumpSustainActive_ = false;
        }
    }

    void Apply(float dt, const Vector3 &gravityDirection, Vector3 &gravityVelocity) {
        dt = std::clamp(dt, 0.0f, 0.1f);
        const Vector3 up = (-gravityDirection).Normalize();

        if (jumpRequested_) {
            gravityVelocity = up * jumpPower_;
            jumpRequested_ = false;
            jumpSustainActive_ = true;
            jumpSustainElapsed_ = 0.0f;
            ++jumpCount_;
        }

        if (!jumpSustainActive_ || !jumpInputHeld_) {
            return;
        }

        if (jumpSustainElapsed_ >= maxJumpInputHoldTime_) {
            jumpSustainActive_ = false;
            return;
        }

        gravityVelocity = up * jumpPower_ * (1.0f - (Normalize01(jumpSustainElapsed_, 0.0f, maxJumpInputHoldTime_) * jumpHoldSpeedScale_));
        jumpSustainElapsed_ = std::min(maxJumpInputHoldTime_, jumpSustainElapsed_ + dt);
    }

    void ResetJumpCount() {
        jumpRequested_ = false;
        jumpSustainActive_ = false;
        jumpInputHeld_ = false;
        jumpSustainElapsed_ = 0.0f;
        jumpCount_ = 0;
    }

    int GetJumpCount() const { return jumpCount_; }
    int GetMaxJumpCount() const { return maxJumpCount_; }

    float GetJumpPower() const { return jumpPower_; }
    void SetJumpPower(float v) { jumpPower_ = v; }
    float GetMaxJumpInputHoldTime() const { return maxJumpInputHoldTime_; }
    void SetMaxJumpInputHoldTime(float v) { maxJumpInputHoldTime_ = std::max(0.0f, v); }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    float jumpPower_ = 50.0f;
    float maxJumpInputHoldTime_ = 0.3f;
    float jumpHoldSpeedScale_ = 0.3f;
    int maxJumpCount_ = 2;
    int jumpCount_ = 0;
    bool jumpRequested_ = false;
    bool jumpInputHeld_ = false;
    bool jumpSustainActive_ = false;
    float jumpSustainElapsed_ = 0.0f;
};

} // namespace KashipanEngine
