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
        auto modelHandle = ModelManager::GetModelDataFromFileName("Player.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetName("CarModel");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 5.0f, 0.0f));
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
        float t = Normalize01(elapsedTime_, easingStartTime_, durationTime_);

        if (auto *tr = carModel_->GetComponent3D<Transform3D>()) {
            Vector3 newTranslate = EaseOutBack(fromTranslate_, toTranslate_, t);
            tr->SetTranslate(newTranslate);
        }

        if (t >= 1.0f) {
            t = 1.0f;
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

    Model *carModel_ = nullptr;

    Vector3 fromTranslate_ = Vector3(32.0f, 0.0f, 0.0f);
    Vector3 toTranslate_ = Vector3(0.0f, 0.0f, 0.0f);
    
    const float durationTime_ = 2.0f;
    const float easingStartTime_ = 1.0f;
    float elapsedTime_ = 0.0f;

    bool isAnimating_ = false;
    bool isFinished_ = false;
    bool isFinishedTriggered_ = false;
};

} // namespace KashipanEngine
