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
    /// @brief カメラを制御するコンポーネントを作成する
    /// @param camera 制御対象の Camera3D
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
    /// @brief デストラクタ
    ~CameraController() override = default;

    /// @brief 毎フレームの更新。ターゲット位置/回転/FOV に向かって補間する
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

    /// @brief 目標の位置を設定する
    /// @param v 目標位置
    void SetTargetTranslate(const Vector3 &v) { targetTranslate_ = v; }
    /// @brief 目標の回転を設定する
    /// @param v 目標回転
    void SetTargetRotate(const Vector3 &v) { targetRotate_ = v; }
    /// @brief 目標の FOV(Y) を設定する
    /// @param v 目標 FOV
    void SetTargetFovY(float v) { targetFovY_ = v; }

    /// @brief 補間の係数を設定する
    /// @param t 補間係数（0..1）
    void SetLerpFactor(float t) { lerpFactor_ = t; }
    /// @brief 補間の係数を取得する
    float GetLerpFactor() const { return lerpFactor_; }

    /// @brief フォロー対象を設定する
    /// @param target フォロー対象オブジェクト
    void SetFollowTarget(Object3DBase *target) { followTarget_ = target; }
    /// @brief フォロー対象を取得する
    Object3DBase *GetFollowTarget() const { return followTarget_; }

    /// @brief フォローオフセットを設定する
    /// @param v オフセットベクトル
    void SetFollowOffset(const Vector3 &v) { followOffset_ = v; }
    /// @brief フォローオフセットを取得する
    const Vector3 &GetFollowOffset() const { return followOffset_; }

    /// @brief 目標位置を取得する
    const Vector3 &GetTargetTranslate() const { return targetTranslate_; }
    /// @brief 目標回転を取得する
    const Vector3 &GetTargetRotate() const { return targetRotate_; }
    /// @brief 目標の FOV を取得する
    float GetTargetFovY() const { return targetFovY_; }

    /// @brief 現在のカメラ位置からフォローオフセットを再計算する
    void RecalculateOffsetFromCurrentCamera() {
        if (!camera_ || !followTarget_) return;

        auto *camTr = camera_->GetComponent3D<Transform3D>();
        auto *tgtTr = followTarget_->GetComponent3D<Transform3D>();
        if (!camTr || !tgtTr) return;

        followOffset_ = camTr->GetTranslate() - tgtTr->GetTranslate();
    }

    /// @brief カメラシェイクを実行する
    /// @param amplitude 振幅
    /// @param durationSec 継続時間（秒）
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