#pragma once
#include <KashipanEngine.h>
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
                targetRotateQuaternion_ = tr->GetRotateQuaternion();
            }
            targetFovY_ = camera_->GetFovY();
        }
    }
    ~CameraController() override = default;

    void Update() override {
        if (!camera_) return;

        const float dt = std::max(0.0f, GetDeltaTime() * GetGameSpeed());
        const float tMove = std::clamp(lerpFactorMove_ * dt * 60.0f, 0.0f, 1.0f);
        const float tRotate = std::clamp(lerpFactorRotate_ * dt * 60.0f, 0.0f, 1.0f);
        const float tFov = std::clamp(lerpFactorFov_ * dt * 60.0f, 0.0f, 1.0f);

        Vector3 desiredTranslate = targetTranslate_;
        if (followTarget_) {
            if (auto *tgtTr = followTarget_->GetComponent3D<Transform3D>()) {
                Matrix4x4 tgtWorldMat = tgtTr->GetWorldMatrix();
                Vector3 tgtPos = Vector3(tgtWorldMat.m[3][0], tgtWorldMat.m[3][1], tgtWorldMat.m[3][2]);
                desiredTranslate = tgtPos + followOffset_;
            }
        }

        if (shakeTimeRemaining_ > 0.0f) {
            shakeTimeRemaining_ = std::max(0.0f, shakeTimeRemaining_ - dt);

            const float tShake = (shakeDuration_ > 0.0f) ? (1.0f - (shakeTimeRemaining_ / shakeDuration_)) : 1.0f;
            const float amp = shakeAmplitude_ * (1.0f - tShake);

            const float phase = shakeTimeRemaining_ * 60.0f;
            if (isShakeX_) desiredTranslate.x += std::sin(phase * 2.3f) * amp;
            if (isShakeY_) desiredTranslate.y += std::sin(phase * 3.7f) * amp;
            if (isShakeZ_) desiredTranslate.z += std::sin(phase * 4.9f) * amp;
        }

        if (auto *tr = camera_->GetComponent3D<Transform3D>()) {
            const Vector3 curT = tr->GetTranslate();

            tr->SetTranslate(Vector3::Lerp(curT, desiredTranslate, tMove));
            if (useQuaternionRotation_) {
                const Quaternion curQ = tr->GetRotateQuaternion();
                tr->SetRotateQuaternion(Quaternion::Slerp(curQ, targetRotateQuaternion_, tRotate));
            } else {
                const Vector3 curR = tr->GetRotate();
                tr->SetRotate(Vector3::Lerp(curR, targetRotate_, tRotate));
            }
        }

        const float curF = camera_->GetFovY();
        camera_->SetFovY(curF + (targetFovY_ - curF) * tFov);
    }

    void SetTargetTranslate(const Vector3 &v) { targetTranslate_ = v; }
    void SetTargetRotate(const Vector3 &v) {
        targetRotate_ = v;
        useQuaternionRotation_ = false;
    }
    void SetTargetRotateQuaternion(const Quaternion &q) {
        targetRotateQuaternion_ = q.Normalize();
        useQuaternionRotation_ = true;
    }
    void SetTargetFovY(float v) { targetFovY_ = v; }

    void SetLerpFactor(float t) {
        lerpFactorMove_ = t;
        lerpFactorRotate_ = t;
        lerpFactorFov_ = t;
    }
    void SetLerpFactorMove(float t) { lerpFactorMove_ = t; }
    void SetLerpFactorRotate(float t) { lerpFactorRotate_ = t; }
    void SetLerpFactorFov(float t) { lerpFactorFov_ = t; }
    float GetLerpFactor() const { return lerpFactorMove_; }
    float GetLerpFactorMove() const { return lerpFactorMove_; }
    float GetLerpFactorRotate() const { return lerpFactorRotate_; }
    float GetLerpFactorFov() const { return lerpFactorFov_; }

    void SetFollowTarget(Object3DBase *target) { followTarget_ = target; }
    Object3DBase *GetFollowTarget() const { return followTarget_; }

    void SetFollowOffset(const Vector3 &v) { followOffset_ = v; }
    const Vector3 &GetFollowOffset() const { return followOffset_; }

    const Vector3 &GetTargetTranslate() const { return targetTranslate_; }
    const Vector3 &GetTargetRotate() const { return targetRotate_; }
    const Quaternion &GetTargetRotateQuaternion() const { return targetRotateQuaternion_; }
    float GetTargetFovY() const { return targetFovY_; }

    void SetIsShakeX(bool v) { isShakeX_ = v; }
    void SetIsShakeY(bool v) { isShakeY_ = v; }
    void SetIsShakeZ(bool v) { isShakeZ_ = v; }

    bool IsShakeX() const { return isShakeX_; }
    bool IsShakeY() const { return isShakeY_; }
    bool IsShakeZ() const { return isShakeZ_; }

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

#if defined(USE_IMGUI)
    void ShowImGui() {
        if (ImGui::CollapsingHeader("CameraController")) {
            ImGui::InputFloat3("Target Translate", &targetTranslate_.x);
            ImGui::InputFloat3("Target Rotate", &targetRotate_.x);
            ImGui::InputFloat("Target FovY", &targetFovY_);
            ImGui::Separator();
            ImGui::InputFloat("Lerp Move", &lerpFactorMove_);
            ImGui::InputFloat("Lerp Rotate", &lerpFactorRotate_);
            ImGui::InputFloat("Lerp Fov", &lerpFactorFov_);
            ImGui::Separator();
            ImGui::InputFloat3("Follow Offset", &followOffset_.x);
            ImGui::Checkbox("Shake X", &isShakeX_);
            ImGui::Checkbox("Shake Y", &isShakeY_);
            ImGui::Checkbox("Shake Z", &isShakeZ_);
            if (ImGui::Button("Recalculate Offset From Current Camera")) {
                RecalculateOffsetFromCurrentCamera();
            }
            ImGui::Separator();
            float shakeAmp = shakeAmplitude_;
            float shakeDur = shakeDuration_;
            if (ImGui::InputFloat("Shake Amplitude", &shakeAmp) && shakeAmp >= 0.0f) {
                shakeAmplitude_ = shakeAmp;
            }
            if (ImGui::InputFloat("Shake Duration", &shakeDur) && shakeDur >= 0.0f) {
                shakeDuration_ = shakeDur;
            }
            if (ImGui::Button("Start Shake")) {
                Shake(shakeAmplitude_, shakeDuration_);
            }
        }
    }
#endif

private:
    Camera3D *camera_ = nullptr;

    Object3DBase *followTarget_ = nullptr;

    Vector3 followOffset_{0.0f, 0.0f, 0.0f};

    Vector3 targetTranslate_{0.0f, 0.0f, 0.0f};
    Vector3 targetRotate_{0.0f, 0.0f, 0.0f};
    Quaternion targetRotateQuaternion_ = Quaternion::Identity();
    float targetFovY_ = 0.7f;

    bool useQuaternionRotation_ = false;

    float lerpFactorMove_ = 0.1f;
    float lerpFactorRotate_ = 0.1f;
    float lerpFactorFov_ = 0.1f;

    float shakeAmplitude_ = 0.0f;
    float shakeDuration_ = 0.0f;
    float shakeTimeRemaining_ = 0.0f;
    bool isShakeX_ = true;
    bool isShakeY_ = true;
    bool isShakeZ_ = false;
};

}  // namespace KashipanEngine