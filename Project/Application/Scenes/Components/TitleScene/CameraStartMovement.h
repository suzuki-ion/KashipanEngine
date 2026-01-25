#pragma once
#include <KashipanEngine.h>
#include <algorithm>

namespace KashipanEngine {

class CameraStartMovement final : public ISceneComponent {
public:
    CameraStartMovement() : ISceneComponent("CameraStartMovement") {}

    void Initialize() override {
        auto *ownerContext = GetOwnerContext();
        auto *sceneDefaultVariables = ownerContext->GetComponent<SceneDefaultVariables>();
        camera3D_ = sceneDefaultVariables ? sceneDefaultVariables->GetMainCamera3D() : nullptr;
        if (camera3D_) {
            if (auto *tr = camera3D_->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(cameraFromTranslate_);
                tr->SetRotate(cameraFromRotate_);
            }
        }
    }

    void Finalize() override {
    }

    void Update() override {
        if (!isAnimating_) return;

        elapsedTime_ += GetDeltaTime();
        float t = Normalize01(elapsedTime_, easingStartTime_, easingEndTime_);

        if (camera3D_) {
            if (auto *tr = camera3D_->GetComponent3D<Transform3D>()) {
                Vector3 newTranslate = EaseInOutCubic(cameraFromTranslate_, cameraToTranslate_, t);
                Vector3 newRotate = EaseInOutCubic(cameraFromRotate_, cameraToRotate_, t);
                tr->SetTranslate(newTranslate);
                tr->SetRotate(newRotate);
            }
        }

        if (elapsedTime_ >= durationTime_) {
            isAnimating_ = false;
            isFinished_ = true;
        }
    }

    void StartAnimation() {
        if (isAnimating_) return;
        isFinishedTriggered_ = false;
        isFinished_ = false;
        isAnimating_ = true;
        elapsedTime_ = 0.0f;
    }

    void Reset() {
        if (camera3D_) {
            if (auto *tr = camera3D_->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(cameraFromTranslate_);
                tr->SetRotate(cameraFromRotate_);
            }
        }
        isAnimating_ = false;
        isFinished_ = false;
        isFinishedTriggered_ = false;
        elapsedTime_ = 0.0f;
    }

    bool IsAnimating() const { return isAnimating_; }
    bool IsFinished() const { return isFinished_; }
    bool IsFinishedTriggered() {
        if (isFinished_ && !isFinishedTriggered_) {
            isFinishedTriggered_ = true;
            return true;
        }
        return false;
    }

private:
    Camera3D *camera3D_ = nullptr;

    Vector3 cameraFromTranslate_ = Vector3(0.0f, 24.0f, -21.0f);
    Vector3 cameraToTranslate_ = Vector3(0.0f, 28.0f, -21.0f);
    Vector3 cameraFromRotate_ = Vector3(0.6f, 0.0f, 0.0f);
    Vector3 cameraToRotate_ = Vector3(0.0f, 0.0f, 0.0f);

    bool isAnimating_ = false;
    bool isFinished_ = false;
    bool isFinishedTriggered_ = false;
    const float durationTime_ = 10.0f;
    const float easingStartTime_ = 0.0f;
    const float easingEndTime_ = 6.0f;
    float elapsedTime_ = 0.0f;
};

} // namespace KashipanEngine
