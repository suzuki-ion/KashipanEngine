#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/PlayerMovementControllerAccess.h"

namespace KashipanEngine {

class PlayerLateralMoveBehavior final : public IObjectComponent3D {
public:
    PlayerLateralMoveBehavior() : IObjectComponent3D("PlayerLateralMoveBehavior", 1) {}
    ~PlayerLateralMoveBehavior() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<PlayerLateralMoveBehavior>();
    }

    void MoveRight(float value = 1.0f) {
        lateralInput_ = std::min(lateralInput_, -value);
    }

    void MoveLeft(float value = 1.0f) {
        lateralInput_ = std::max(lateralInput_, value);
    }

    void Apply(float dt, const Vector3 &gravityDirection, const Vector3 &forwardDirection, float forwardSpeed, float minForwardSpeed) {
        const Vector3 down = gravityDirection.Normalize();
        Vector3 right = down.Cross(forwardDirection);
        if (right.LengthSquared() <= 0.000001f) {
            right = Vector3{1.0f, 0.0f, 0.0f};
        } else {
            right = right.Normalize();
        }

        const float boostedLateralMaxSpeed = lateralMaxSpeed_
            + std::max(0.0f, forwardSpeed - minForwardSpeed) * lateralSpeedPerForward_;

        const Vector3 desired = right * (std::clamp(lateralInput_, -1.0f, 1.0f) * boostedLateralMaxSpeed);
        const float lerpT = std::clamp(lateralAcceleration_ * dt, 0.0f, 1.0f);
        lateralVelocity_ = Vector3::Lerp(lateralVelocity_, desired, lerpT);
        lateralInput_ = 0.0f;
    }

    const Vector3 &GetLateralVelocity() const { return lateralVelocity_; }
    Vector3 &LateralVelocityRef() { return lateralVelocity_; }
    float GetLateralMaxSpeed() const { return lateralMaxSpeed_; }
    float GetLateralAcceleration() const { return lateralAcceleration_; }
    float GetLateralSpeedPerForward() const { return lateralSpeedPerForward_; }
    float GetLateralInput() const { return lateralInput_; }
    void ClearLateralInput() { lateralInput_ = 0.0f; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    Vector3 lateralVelocity_{0.0f, 0.0f, 0.0f};
    float lateralInput_ = 0.0f;
    float lateralMaxSpeed_ = 16.0f;
    float lateralAcceleration_ = 8.0f;
    float lateralSpeedPerForward_ = 0.25f;
};

} // namespace KashipanEngine
