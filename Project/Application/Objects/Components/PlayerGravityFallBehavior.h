#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/PlayerMovementControllerAccess.h"

namespace KashipanEngine {

class PlayerGravityFallBehavior final : public IObjectComponent3D {
public:
    PlayerGravityFallBehavior() : IObjectComponent3D("PlayerGravityFallBehavior", 1) {}
    ~PlayerGravityFallBehavior() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<PlayerGravityFallBehavior>();
    }

    void Apply(float dt, const Vector3 &gravityDirection) {
        const Vector3 down = gravityDirection.Normalize();
        gravityVelocity_ += down * (gravityPower_ * dt);
    }

    Vector3 &GravityVelocityRef() { return gravityVelocity_; }
    const Vector3 &GetGravityVelocity() const { return gravityVelocity_; }
    void SetGravityVelocity(const Vector3 &v) { gravityVelocity_ = v; }

    float GetGravityPower() const { return gravityPower_; }
    void SetGravityPower(float v) { gravityPower_ = v; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    Vector3 gravityVelocity_{0.0f, 0.0f, 0.0f};
    float gravityPower_ = 128.0f;
};

} // namespace KashipanEngine
