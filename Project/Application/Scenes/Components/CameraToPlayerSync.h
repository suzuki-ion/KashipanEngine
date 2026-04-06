#pragma once

#include <KashipanEngine.h>
#include "Scenes/Components/CameraController.h"
#include "Objects/Components/PlayerMovement.h"

#include <algorithm>
#include <cmath>

namespace KashipanEngine {

class CameraToPlayerSync final : public ISceneComponent {
public:
    explicit CameraToPlayerSync(Object3DBase *player)
        : ISceneComponent("CameraToPlayerSync", 1), player_(player) {}

    ~CameraToPlayerSync() override = default;

    void Update() override {
        if (!player_) return;

        auto *playerTr = player_->GetComponent3D<Transform3D>();
        auto *playerMovement = player_->GetComponent3D<PlayerMovement>();
        auto *cameraController = GetOwnerContext() ? GetOwnerContext()->GetComponent<CameraController>() : nullptr;

        if (!playerTr || !playerMovement || !cameraController) return;

        const Vector3 playerPos = playerTr->GetTranslate();
        const Vector3 gravity = playerMovement->GetGravityDirection().Normalize();
        const Vector3 up = -gravity;
        const Vector3 forward = playerMovement->GetForwardDirection().Normalize();

        const Vector3 cameraPos = playerPos - forward * followDistance_ + up * followHeight_;
        const Vector3 lookAt = playerPos + up * lookAtHeight_;

        const Vector3 lookDir = (lookAt - cameraPos).Normalize();

        const float minSpeed = playerMovement->GetMinForwardSpeed();
        const float maxSpeed = playerMovement->GetMaxForwardSpeed();
        float speedRatio = 0.0f;
        if (maxSpeed > minSpeed) {
            speedRatio = (playerMovement->GetForwardSpeed() - minSpeed) / (maxSpeed - minSpeed);
        }
        speedRatio = std::clamp(speedRatio, 0.0f, 1.0f);
        const float targetFov = 0.7f + (1.5f - 0.7f) * speedRatio;

        cameraController->SetTargetTranslate(cameraPos);
        cameraController->SetTargetRotateQuaternion(ComputeQuaternionFromForwardUp(lookDir, up));
        cameraController->SetTargetFovY(targetFov);
        cameraController->SetLerpFactor(std::clamp(baseLerpFactor_ * GetGameSpeed(), 0.0f, 1.0f));
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

    float followDistance_ = 8.0f;
    float followHeight_ = 2.0f;
    float lookAtHeight_ = 1.0f;
    float baseLerpFactor_ = 0.2f;
};

} // namespace KashipanEngine
