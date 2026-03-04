#pragma once

#include "Scene/Components/ISceneComponent.h"
#include "Objects/Object3DBase.h"
#include "Objects/SystemObjects/Camera3D.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Math/Vector3.h"
#include "Math/Quaternion.h"

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
                // 初期回転をオイラー角からクォータニオンに変換
                targetRotateQuat_ = EulerToQuaternion(tr->GetRotate());
                currentRotateQuat_ = targetRotateQuat_;
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
            if (isShakeX_) desiredTranslate.x += std::sin(phase * 2.3f) * amp;
            if (isShakeY_) desiredTranslate.y += std::sin(phase * 3.7f) * amp;
            if (isShakeZ_) desiredTranslate.z += std::sin(phase * 4.9f) * amp;
        }

        if (auto *tr = camera_->GetComponent3D<Transform3D>()) {
            const Vector3 curT = tr->GetTranslate();

            tr->SetTranslate(Vector3::Lerp(curT, desiredTranslate, t));

            // クォータニオンSlerpで回転を補間（ジンバルロック回避＆最短経路回転）
            currentRotateQuat_ = Quaternion::Slerp(currentRotateQuat_, targetRotateQuat_, t);
            tr->SetRotateQuaternion(currentRotateQuat_);
        }

        const float curF = camera_->GetFovY();
        camera_->SetFovY(curF + (targetFovY_ - curF) * t);
    }

    void SetTargetTranslate(const Vector3 &v) { targetTranslate_ = v; }

    void SetTargetRotate(const Vector3 &v) {
        targetRotateQuat_ = EulerToQuaternion(v);
    }

    void SetTargetRotateQuaternion(const Quaternion &q) {
        targetRotateQuat_ = q;
    }

    void SetTargetFovY(float v) { targetFovY_ = v; }

    void SetLerpFactor(float t) { lerpFactor_ = t; }
    float GetLerpFactor() const { return lerpFactor_; }

    void SetFollowTarget(Object3DBase *target) { followTarget_ = target; }
    Object3DBase *GetFollowTarget() const { return followTarget_; }

    void SetFollowOffset(const Vector3 &v) { followOffset_ = v; }
    const Vector3 &GetFollowOffset() const { return followOffset_; }

    const Vector3 &GetTargetTranslate() const { return targetTranslate_; }
    const Quaternion &GetTargetRotateQuaternion() const { return targetRotateQuat_; }
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
            Vector3 euler = QuaternionToEuler(targetRotateQuat_);
            if (ImGui::InputFloat3("Target Rotate", &euler.x)) {
                targetRotateQuat_ = EulerToQuaternion(euler);
            }
            ImGui::InputFloat("Target FovY", &targetFovY_);
            ImGui::Separator();
            ImGui::InputFloat("Lerp Factor", &lerpFactor_);
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
    // オイラー角からクォータニオンへの変換（XYZ回転順）
    static Quaternion EulerToQuaternion(const Vector3 &euler) {
        Quaternion q;
        Quaternion qx = q.MakeRotateAxisAngle(Vector3(1.0f, 0.0f, 0.0f), euler.x);
        Quaternion qy = q.MakeRotateAxisAngle(Vector3(0.0f, 1.0f, 0.0f), euler.y);
        Quaternion qz = q.MakeRotateAxisAngle(Vector3(0.0f, 0.0f, 1.0f), euler.z);
        return (qx * qy * qz).Normalize();
    }

    // クォータニオンからオイラー角への変換（XYZ回転順）
    static Vector3 QuaternionToEuler(const Quaternion &q) {
        // 回転行列経由で変換
        Matrix4x4 m = q.MakeRotateMatrix();
        float rotX, rotY, rotZ;

        // m[0][2] = sin(Y) in XYZ order
        float sy = m.m[0][2];
        if (sy > 0.9999f) {
            rotY = 3.14159265f * 0.5f;
            rotX = std::atan2(m.m[1][0], m.m[1][1]);
            rotZ = 0.0f;
        } else if (sy < -0.9999f) {
            rotY = -3.14159265f * 0.5f;
            rotX = std::atan2(-m.m[1][0], m.m[1][1]);
            rotZ = 0.0f;
        } else {
            rotY = std::asin(sy);
            rotX = std::atan2(-m.m[1][2], m.m[2][2]);
            rotZ = std::atan2(-m.m[0][1], m.m[0][0]);
        }
        return Vector3(rotX, rotY, rotZ);
    }

    Camera3D *camera_ = nullptr;

    Object3DBase *followTarget_ = nullptr;

    Vector3 followOffset_{0.0f, 0.0f, 0.0f};

    Vector3 targetTranslate_{0.0f, 0.0f, 0.0f};
    Quaternion targetRotateQuat_ = Quaternion::Identity();
    Quaternion currentRotateQuat_ = Quaternion::Identity();
    float targetFovY_ = 0.7f;

    float lerpFactor_ = 0.1f;

    float shakeAmplitude_ = 0.0f;
    float shakeDuration_ = 0.0f;
    float shakeTimeRemaining_ = 0.0f;
    bool isShakeX_ = true;
    bool isShakeY_ = true;
    bool isShakeZ_ = false;
};

}  // namespace KashipanEngine}  // namespace KashipanEngine