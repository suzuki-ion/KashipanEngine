#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/PlayerMovementControllerAccess.h"

namespace KashipanEngine {

class PlayerForwardMoveBehavior final : public IObjectComponent3D {
public:
    PlayerForwardMoveBehavior() : IObjectComponent3D("PlayerForwardMoveBehavior", 1) {}
    ~PlayerForwardMoveBehavior() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<PlayerForwardMoveBehavior>();
    }

    void Apply(float dt, bool grounded, const Vector3 &gravityVelocity, const Vector3 &gravityDirection) {
        if (grounded) {
            forwardSpeed_ = std::max(minForwardSpeed_, forwardSpeed_ - groundDeceleration_ * dt);
            return;
        }

        const Vector3 down = gravityDirection.Normalize();
        const float fallSpeed = gravityVelocity.Dot(down);
        if (fallSpeed > 0.0f) {
            const float accel = forwardAcceleration_ * (1.0f + fallSpeed * forwardAccelPerFallSpeed_);
            forwardSpeed_ = std::min(maxForwardSpeed_, forwardSpeed_ + accel * dt);
        }
    }

    float &ForwardSpeedRef() { return forwardSpeed_; }
    float GetForwardSpeed() const { return forwardSpeed_; }
    float GetMinForwardSpeed() const { return minForwardSpeed_; }
    float GetMaxForwardSpeed() const { return maxForwardSpeed_; }
    float GetForwardAcceleration() const { return forwardAcceleration_; }
    float GetForwardAccelPerFallSpeed() const { return forwardAccelPerFallSpeed_; }
    float GetGroundDeceleration() const { return groundDeceleration_; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    float forwardSpeed_ = 48.0f;
    float minForwardSpeed_ = 16.0f;
    float maxForwardSpeed_ = 96.0f;
    float forwardAcceleration_ = 1.0f;
    float forwardAccelPerFallSpeed_ = 0.05f;
    float groundDeceleration_ = 16.0f;
};

} // namespace KashipanEngine
