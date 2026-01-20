#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class StartTextUpdate final : public ISceneComponent {
public:
    StartTextUpdate(InputCommand *inputCommand)
        : ISceneComponent("StartTextUpdate"), inputCommand_(inputCommand) {}

    void Initialize() override {
        auto *ownerContext = GetOwnerContext();
        auto *sceneDefaultVariables = ownerContext->GetComponent<SceneDefaultVariables>();
        auto *screenBuffer3D = sceneDefaultVariables->GetScreenBuffer3D();
        auto *shadowMapBuffer = sceneDefaultVariables->GetShadowMapBuffer();

        // スタートテキストモデル
        auto modelHandle = ModelManager::GetModelDataFromFileName("startText.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetName("StartText");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 4.0f, 0.0f));
            tr->SetRotate(Vector3(0.6f, 0.0f, 0.0f));
        }
        startTextModel_ = obj.get();
        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        ownerContext->AddObject3D(std::move(obj));
    }

    void Finalize() override {
    }

    void Update() override {
        if (!inputCommand_) return;
        if (inputCommand_->Evaluate("Submit").Triggered()) {
            StartTextAnimation();
        }

        if (!isAnimating_) return;

        elapsedTime_ += GetDeltaTime();
        float t = Normalize01(elapsedTime_, 0.0f, durationTime_);

        if (auto *tr = startTextModel_->GetComponent3D<Transform3D>()) {
            Vector3 newScale = EaseOutExpo(fromScale_, toScale_, t);
            tr->SetScale(newScale);
        }

        if (t >= 1.0f) {
            t = 1.0f;
            isAnimating_ = false;
            isFinished_ = true;
        }
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
    void StartTextAnimation() {
        if (isAnimating_) return;
        isFinishedTriggered_ = false;
        isFinished_ = false;
        isAnimating_ = true;
        elapsedTime_ = 0.0f;
    }

    InputCommand *inputCommand_ = nullptr;
    Model *startTextModel_ = nullptr;

    Vector3 fromScale_ = Vector3(1.0f, 1.0f, 1.0f);
    Vector3 toScale_ = Vector3(8.0f, 0.0f, 0.0f);

    bool isAnimating_ = false;
    bool isFinished_ = false;
    bool isFinishedTriggered_ = false;
    const float durationTime_ = 0.1f;
    float elapsedTime_ = 0.0f;
};

} // namespace KashipanEngine
