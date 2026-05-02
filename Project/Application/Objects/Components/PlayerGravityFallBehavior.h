#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/PlayerMovementControllerAccess.h"

#include <algorithm>

namespace KashipanEngine {

class PlayerGravityFallBehavior final : public IObjectComponent3D {
public:
    PlayerGravityFallBehavior() : IObjectComponent3D("PlayerGravityFallBehavior", 1) {}
    ~PlayerGravityFallBehavior() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<PlayerGravityFallBehavior>();
    }

    void Apply(float dt, const Vector3 &gravityDirection) {
        dt = std::clamp(dt, 0.0f, 0.1f);
        const Vector3 down = gravityDirection.Normalize();
        const float power = fastFallEnabled_ ? gravityPower_ * fastFallMultiplier_ : gravityPower_;
        gravityVelocity_ += down * (power * dt);
        const float gravitySpeed = gravityVelocity_.Length();
        const float maxGravitySpeed = fastFallEnabled_ ? maxGravitySpeed_ * fastFallMultiplier_ : maxGravitySpeed_;
        if (gravitySpeed > maxGravitySpeed) {
            gravityVelocity_ = gravityVelocity_ * (maxGravitySpeed / gravitySpeed);
        }
    }

    Vector3 &GravityVelocityRef() { return gravityVelocity_; }
    const Vector3 &GetGravityVelocity() const { return gravityVelocity_; }
    void SetGravityVelocity(const Vector3 &v) { gravityVelocity_ = v; }
    void SetFastFallEnabled(bool enabled) { fastFallEnabled_ = enabled; }
    bool IsFastFallEnabled() const { return fastFallEnabled_; }

    float GetGravityPower() const { return gravityPower_; }
    void SetGravityPower(float v) { gravityPower_ = v; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    Vector3 gravityVelocity_{0.0f, 0.0f, 0.0f};
    float gravityPower_ = 220.0f;
    float maxGravitySpeed_ = 64.0f;
    float fastFallMultiplier_ = 2.0f;
    bool fastFallEnabled_ = false;
};

} // namespace KashipanEngine
