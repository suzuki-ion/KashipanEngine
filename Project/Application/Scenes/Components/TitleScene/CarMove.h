#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class CarMove final : public ISceneComponent {
public:
    CarMove() : ISceneComponent("CarMove") {}

    void Initialize() override {
        auto *ownerContext = GetOwnerContext();
        auto *sceneDefaultVariables = ownerContext->GetComponent<SceneDefaultVariables>();
        auto *screenBuffer3D = sceneDefaultVariables->GetScreenBuffer3D();
        auto *shadowMapBuffer = sceneDefaultVariables->GetShadowMapBuffer();

        // 車モデル
        auto modelHandle = ModelManager::GetModelDataFromFileName("player.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetName("CarModel");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(moveInFrom_);
        }
        carModel_ = obj.get();
        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        ownerContext->AddObject3D(std::move(obj));
    }

    void Finalize() override {
    }

    void Update() override {
        if (!isAnimating_) return;
        elapsedTime_ += GetDeltaTime();
        MoveIn();
        MoveOut();
    }

    void StartMoveIn() {
        if (isAnimating_) return;
        isMoveIn_ = true;
        StartAnimation();
    }

    void StartMoveOut() {
        if (isAnimating_) return;
        isMoveIn_ = false;
        StartAnimation();
    }

    void Reset() {
        if (carModel_) {
            if (auto *tr = carModel_->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(moveInFrom_);
            }
        }
        isAnimating_ = false;
        isFinished_ = false;
        isFinishedTriggered_ = false;
        elapsedTime_ = 0.0f;
        isMoveIn_ = true;
    }

    bool IsAnimating() const { return isAnimating_; }
    bool IsMoveIn() const { return isMoveIn_; }
    bool IsFinished() const { return isFinished_; }
    bool IsFinishedTriggered() {
        if (isFinished_ && !isFinishedTriggered_) {
            isFinishedTriggered_ = true;
            return true;
        }
        return false;
    }

private:
    void StartAnimation() {
        isFinishedTriggered_ = false;
        isFinished_ = false;
        isAnimating_ = true;
        elapsedTime_ = 0.0f;
    }

    void MoveIn() {
        if (!isMoveIn_) return;
        const float t = Normalize01(elapsedTime_, moveInEasingStartTime_, moveInEasingEndTime_);

        if (auto *tr = carModel_->GetComponent3D<Transform3D>()) {
            Vector3 newTranslate;
            newTranslate = EaseOutBack(moveInFrom_, moveInTo_, t);
            tr->SetTranslate(newTranslate);
        }

        if (elapsedTime_ >= moveInDurationTime_) {
            isAnimating_ = false;
            isFinished_ = true;
        }
    }

    void MoveOut() {
        if (isMoveIn_) return;
        const float t = Normalize01(elapsedTime_, moveOutEasingStartTime_, moveOutEasingEndTime_);

        if (auto *tr = carModel_->GetComponent3D<Transform3D>()) {
            Vector3 newTranslate;
            newTranslate = EaseInBack(moveOutFrom_, moveOutTo_, t);
            tr->SetTranslate(newTranslate);
        }

        if (elapsedTime_ >= moveOutDurationTime_) {
            isAnimating_ = false;
            isFinished_ = true;
        }
    }

    Model *carModel_ = nullptr;

    Vector3 moveInFrom_ = Vector3(32.0f, 0.0f, 0.0f);
    Vector3 moveInTo_ = Vector3(0.0f, 0.0f, 0.0f);

    Vector3 moveOutFrom_ = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 moveOutTo_ = Vector3(-32.0f, 0.0f, 0.0f);
    
    const float moveInDurationTime_ = 2.0f;
    const float moveInEasingStartTime_ = 1.0f;
    const float moveInEasingEndTime_ = 2.0f;

    const float moveOutDurationTime_ = 3.0f;
    const float moveOutEasingStartTime_ = 1.0f;
    const float moveOutEasingEndTime_ = 2.0f;
    
    float elapsedTime_ = 0.0f;

    bool isAnimating_ = false;
    bool isFinished_ = false;
    bool isFinishedTriggered_ = false;
    bool isMoveIn_ = true;
};

} // namespace KashipanEngine
