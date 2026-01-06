#pragma once

#include "Scene/Components/ISceneComponent.h"
#include "Objects/Object3DBase.h"
#include "Objects/SystemObjects/Camera3D.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Math/Vector3.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace KashipanEngine {

class CameraController final : public ISceneComponent {
public:
    explicit CameraController(Camera3D *camera)
        : ISceneComponent("CameraController", 1), camera_(camera) {
        if (camera_) {
            if (auto *tr = camera_->GetComponent3D<Transform3D>()) {
                targetTranslate_ = tr->GetTranslate();
                targetRotate_ = tr->GetRotate();
            }
            targetFovY_ = camera_->GetFovY();
        }
    }
    ~CameraController() override = default;

    void Update() override {
        if (!camera_) return;

        const float t = std::clamp(lerpFactor_, 0.0f, 1.0f);

        Vector3 desiredTranslate = targetTranslate_;
        if (followTarget_) {
            if (auto *tgtTr = followTarget_->GetComponent3D<Transform3D>()) {
                Matrix4x4 tgtWorldMat = tgtTr->GetWorldMatrix();
                Vector3 tgtPos = Vector3(tgtWorldMat.m[3][0], tgtWorldMat.m[3][1], tgtWorldMat.m[3][2]);
                desiredTranslate = tgtPos + followOffset_;
            }
        }

        if (shakeTimeRemaining_ > 0.0f) {
            const float dt = std::max(0.0f, GetDeltaTime());
            shakeTimeRemaining_ = std::max(0.0f, shakeTimeRemaining_ - dt);

            const float tShake = (shakeDuration_ > 0.0f) ? (1.0f - (shakeTimeRemaining_ / shakeDuration_)) : 1.0f;
            const float amp = shakeAmplitude_ * (1.0f - tShake);

            const float phase = shakeTimeRemaining_ * 60.0f;
            desiredTranslate.x += std::sin(phase * 2.3f) * amp;
            desiredTranslate.y += std::sin(phase * 3.7f) * amp;
            desiredTranslate.z += std::sin(phase * 4.9f) * amp;
        }

        if (auto *tr = camera_->GetComponent3D<Transform3D>()) {
            const Vector3 curT = tr->GetTranslate();
            const Vector3 curR = tr->GetRotate();

            tr->SetTranslate(Vector3::Lerp(curT, desiredTranslate, t));
            tr->SetRotate(Vector3::Lerp(curR, targetRotate_, t));
        }

        const float curF = camera_->GetFovY();
        camera_->SetFovY(curF + (targetFovY_ - curF) * t);
    }

    void SetTargetTranslate(const Vector3 &v) { targetTranslate_ = v; }
    void SetTargetRotate(const Vector3 &v) { targetRotate_ = v; }
    void SetTargetFovY(float v) { targetFovY_ = v; }

    void SetLerpFactor(float t) { lerpFactor_ = t; }
    float GetLerpFactor() const { return lerpFactor_; }

    void SetFollowTarget(Object3DBase *target) { followTarget_ = target; }
    Object3DBase *GetFollowTarget() const { return followTarget_; }

    void SetFollowOffset(const Vector3 &v) { followOffset_ = v; }
    const Vector3 &GetFollowOffset() const { return followOffset_; }

    const Vector3 &GetTargetTranslate() const { return targetTranslate_; }
    const Vector3 &GetTargetRotate() const { return targetRotate_; }
    float GetTargetFovY() const { return targetFovY_; }

    void RecalculateOffsetFromCurrentCamera() {
        if (!camera_ || !followTarget_) return;

        auto *camTr = camera_->GetComponent3D<Transform3D>();
        auto *tgtTr = followTarget_->GetComponent3D<Transform3D>();
        if (!camTr || !tgtTr) return;

        followOffset_ = camTr->GetTranslate() - tgtTr->GetTranslate();
    }

    void Shake(float amplitude, float durationSec) {
        shakeAmplitude_ = std::max(0.0f, amplitude);
        shakeDuration_ = std::max(0.0f, durationSec);
        shakeTimeRemaining_ = shakeDuration_;
    }

private:
    Camera3D *camera_ = nullptr;

    Object3DBase *followTarget_ = nullptr;

    Vector3 followOffset_{0.0f, 0.0f, 0.0f};

    Vector3 targetTranslate_{0.0f, 0.0f, 0.0f};
    Vector3 targetRotate_{0.0f, 0.0f, 0.0f};
    float targetFovY_ = 0.7f;

    float lerpFactor_ = 0.1f;

    float shakeAmplitude_ = 0.0f;
    float shakeDuration_ = 0.0f;
    float shakeTimeRemaining_ = 0.0f;
};

}  // namespace KashipanEngine