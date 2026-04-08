#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/PlayerMovementControllerAccess.h"

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

    void Apply(const Vector3 &gravityDirection, Vector3 &gravityVelocity) {
        if (!jumpRequested_) return;
        gravityVelocity = (-gravityDirection.Normalize()) * jumpPower_;
        jumpRequested_ = false;
        ++jumpCount_;
    }

    void ResetJumpCount() {
        jumpRequested_ = false;
        jumpCount_ = 0;
    }

    int GetJumpCount() const { return jumpCount_; }
    int GetMaxJumpCount() const { return maxJumpCount_; }

    float GetJumpPower() const { return jumpPower_; }
    void SetJumpPower(float v) { jumpPower_ = v; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    float jumpPower_ = 64.0f;
    int maxJumpCount_ = 2;
    int jumpCount_ = 0;
    bool jumpRequested_ = false;
};

} // namespace KashipanEngine
