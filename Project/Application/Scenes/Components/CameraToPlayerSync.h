#pragma once

#include <KashipanEngine.h>
#include "Scenes/Components/CameraController.h"
#include "Scenes/Components/StageGroundGenerator.h"
#include "Objects/Components/PlayerMovementController.h"
#include "Objects/Components/PlayerInputHandler.h"

#include <algorithm>
#include <cmath>

namespace KashipanEngine {

class CameraToPlayerSync final : public ISceneComponent {
public:
    explicit CameraToPlayerSync(Object3DBase *player)
        : ISceneComponent("CameraToPlayerSync", 1), player_(player) {}

    ~CameraToPlayerSync() override = default;

    void SetClearViewEnabled(bool enabled) { clearViewEnabled_ = enabled; }

    void Update() override {
        if (!player_) return;

        auto *playerTr = player_->GetComponent3D<Transform3D>();
        auto *playerMovement = player_->GetComponent3D<PlayerMovementController>();
        auto *cameraController = GetOwnerContext() ? GetOwnerContext()->GetComponent<CameraController>() : nullptr;

        if (!playerTr || !playerMovement || !cameraController) return;

        const Vector3 playerPos = playerTr->GetTranslate();
        const Vector3 gravity = playerMovement->GetGravityDirection().Normalize();
        const Vector3 up = -gravity;
        const Vector3 forward = playerMovement->GetForwardDirection().Normalize();

        const float minSpeed = playerMovement->GetMinForwardSpeed();
        const float maxSpeed = playerMovement->GetMaxForwardSpeed();
        float speedRatio = 0.0f;
        if (maxSpeed > minSpeed) {
            speedRatio = (playerMovement->GetForwardSpeed() - minSpeed) / (maxSpeed - minSpeed);
        }
        speedRatio = std::clamp(speedRatio, 0.0f, 1.0f);

        const float followDistance = std::lerp(followDistanceMin_, followDistanceMax_, speedRatio);
        const float followHeight = std::lerp(followHeightMin_, followHeightMax_, speedRatio);
        const float lookAtHeight = std::lerp(lookAtHeightMin_, lookAtHeightMax_, speedRatio);

        Vector3 cameraPos = playerPos - forward * followDistance + up * followHeight;
        Vector3 lookDir = (playerPos + up * lookAtHeight - cameraPos).Normalize();

        if (clearViewEnabled_) {
            Vector3 right = up.Cross(forward);
            if (right.LengthSquared() <= 0.000001f) {
                right = Vector3{1.0f, 0.0f, 0.0f};
            } else {
                right = right.Normalize();
            }
            cameraPos = playerPos + right * clearViewRightDistance_ + forward * clearViewForwardDistance_ + up * clearViewHeight_;
            const Vector3 lookTarget = playerPos - right * clearViewLookOffsetRight_ + up * lookAtHeight;
            lookDir = (lookTarget - cameraPos).Normalize();
        }

        float targetFov = Lerp(fovMin_, fovMax_, speedRatio);

        auto *inputHandler = player_->GetComponent3D<PlayerInputHandler>();
        const bool isGravitySwitching = inputHandler && inputHandler->IsGravitySwitching();
        if (isGravitySwitching) {
            cameraPos = playerPos - forward * gravitySwitchFollowDistance_;
            targetFov = 0.7f;

            if (const auto &requested = inputHandler->GetRequestedGravityDirection();
                requested.has_value() && requested->LengthSquared() > 0.000001f) {
                const Vector3 gravityAim = requested->Normalize();
                lookDir = (forward + gravityAim).Normalize();
            } else {
                // 重力変更入力が未確定の間は、重力方向へは向けずプレイヤー注視を維持
                lookDir = (playerPos + up * lookAtHeight - cameraPos).Normalize();
            }
        }

        const float fallSpeed = std::max(0.0f, playerMovement->GetGravityVelocity().Dot(gravity));
        const float fallTiltRatio = std::clamp(fallSpeed / std::max(0.0001f, fallSpeedForMaxTilt_), 0.0f, 1.0f);
        if (!isGravitySwitching && fallTiltRatio > 0.0f) {
            const Vector3 lookTarget = playerPos + up * lookAtHeight + gravity * (maxLookDownOffset_ * fallTiltRatio);
            lookDir = (lookTarget - cameraPos).Normalize();
        }

        if (inputHandler && inputHandler->IsRearConfirming()) {
            lookDir = -forward;
        }

        float landingImpact = 0.0f;
        if (playerMovement->ConsumeLandingImpact(landingImpact) && landingImpact > landingImpactThreshold_) {
            const float t = std::clamp((landingImpact - landingImpactThreshold_) / std::max(0.0001f, landingImpactForMaxShake_ - landingImpactThreshold_), 0.0f, 1.0f);
            const float shakeScale = std::pow(t, 0.6f);
            cameraController->Shake(maxShakeAmplitude_ * shakeScale, maxShakeDuration_ * shakeScale);

            if (auto *ctx = GetOwnerContext()) {
                if (auto *groundGenerator = ctx->GetComponent<StageGroundGenerator>()) {
                    const float radius = groundReactionBaseRadius_ + landingImpact * groundReactionRadiusPerImpact_;
                    groundGenerator->TriggerGroundReaction(playerPos, radius);
                }
            }
        }

        cameraController->SetTargetTranslate(cameraPos);
        cameraController->SetTargetRotateQuaternion(ComputeQuaternionFromForwardUp(lookDir, up));
        cameraController->SetTargetFovY(targetFov);
        cameraController->SetLerpFactorMove(1.0f);
        cameraController->SetLerpFactorRotate(0.05f);
        cameraController->SetLerpFactorFov(0.1f);
    }

private:
    static constexpr float kPi = 3.14159265358979323846f;

    static Quaternion MakeFromToQuaternion(const Vector3 &from, const Vector3 &to) {
        const Vector3 f = from.Normalize();
        const Vector3 t = to.Normalize();
        const float dot = std::clamp(f.Dot(t), -1.0f, 1.0f);

        if (dot > 0.9999f) {
            return Quaternion::Identity();
        }

        if (dot < -0.9999f) {
            Vector3 axis = f.Cross(Vector3{1.0f, 0.0f, 0.0f});
            if (axis.LengthSquared() <= 0.000001f) {
                axis = f.Cross(Vector3{0.0f, 1.0f, 0.0f});
            }
            axis = axis.Normalize();
            return Quaternion().MakeRotateAxisAngle(axis, kPi);
        }

        Vector3 axis = f.Cross(t);
        Quaternion q(axis.x, axis.y, axis.z, 1.0f + dot);
        return q.Normalize();
    }

    static Quaternion ComputeQuaternionFromForwardUp(const Vector3 &desiredForward, const Vector3 &desiredUp) {
        const Vector3 defaultForward{0.0f, 0.0f, 1.0f};
        const Vector3 defaultUp{0.0f, 1.0f, 0.0f};

        Quaternion qForward = MakeFromToQuaternion(defaultForward, desiredForward);

        Vector3 currentUp = qForward.RotateVector(defaultUp);
        Vector3 upProjected = desiredUp - desiredForward * desiredUp.Dot(desiredForward);
        Vector3 currentUpProjected = currentUp - desiredForward * currentUp.Dot(desiredForward);

        if (upProjected.LengthSquared() > 0.000001f && currentUpProjected.LengthSquared() > 0.000001f) {
            upProjected = upProjected.Normalize();
            currentUpProjected = currentUpProjected.Normalize();

            const Vector3 cross = currentUpProjected.Cross(upProjected);
            const float dot = std::clamp(currentUpProjected.Dot(upProjected), -1.0f, 1.0f);
            const float angle = std::atan2(cross.Dot(desiredForward), dot);
            const Quaternion qRoll = Quaternion().MakeRotateAxisAngle(desiredForward, angle);
            qForward = (qRoll * qForward).Normalize();
        }

        return qForward;
    }

    Object3DBase *player_ = nullptr;

    float followDistanceMin_ = 4.0f;
    float followDistanceMax_ = 2.0f;
    float followHeightMin_ = 2.0f;
    float followHeightMax_ = 6.0f;
    float lookAtHeightMin_ = 2.0f;
    float lookAtHeightMax_ = 5.0f;
    float fovMin_ = 0.8f;
    float fovMax_ = 2.25f;
    float gravitySwitchFollowDistance_ = 10.0f;
    float fallSpeedForMaxTilt_ = 128.0f;
    float maxLookDownOffset_ = 6.0f;

    float landingImpactThreshold_ = 6.0f;
    float landingImpactForMaxShake_ = 64.0f;
    float maxShakeAmplitude_ = 1.4f;
    float maxShakeDuration_ = 0.5f;

    float groundReactionBaseRadius_ = 8.0f;
    float groundReactionRadiusPerImpact_ = 0.6f;

    bool clearViewEnabled_ = false;
    float clearViewRightDistance_ = 2.0f;
    float clearViewForwardDistance_ = 6.0f;
    float clearViewHeight_ = 2.0f;
    float clearViewLookOffsetRight_ = -1.5f;
};

} // namespace KashipanEngine
