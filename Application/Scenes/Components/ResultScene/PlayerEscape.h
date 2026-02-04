#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class PlayerEscape final : public ISceneComponent {
public:
    PlayerEscape() : ISceneComponent("PlayerEscape") {}

    void Initialize() override {
        auto *ownerContext = GetOwnerContext();
        auto *sceneDefaultVariables = ownerContext->GetComponent<SceneDefaultVariables>();
        auto *screenBuffer3D = sceneDefaultVariables->GetScreenBuffer3D();
        auto *shadowMapBuffer = sceneDefaultVariables->GetShadowMapBuffer();

        auto modelHandle = ModelManager::GetModelDataFromFileName("player.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetName("PlayerModel");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(fromTranslate_);
            tr->SetRotate(Vector3(0.0f, 0.0f, 0.0f));
        }
        playerModel_ = obj.get();
        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        ownerContext->AddObject3D(std::move(obj));
    }

    void Finalize() override {
    }

    void Update() override {
        if (!isAnimating_) return;
        elapsedTime_ += GetDeltaTime();
        Escape();
    }

    void StartEscape() {
        if (isAnimating_) return;
        StartAnimation();
    }

    void EndAnimation() {
        if (playerModel_) {
            if (auto *tr = playerModel_->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(toTranslate_);
            }
        }
        isAnimating_ = false;
        isFinished_ = true;
        isFinishedTriggered_ = true;
        elapsedTime_ = translateDurationTime_;
    }

    void Reset() {
        if (playerModel_) {
            if (auto *tr = playerModel_->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(fromTranslate_);
            }
            SetVisible(true);
        }
        isAnimating_ = false;
        isFinished_ = false;
        isFinishedTriggered_ = false;
        elapsedTime_ = 0.0f;
    }

    void SetVisible(bool visible) {
        if (!playerModel_) return;
        if (auto *mt = playerModel_->GetComponent3D<Material3D>()) {
            Vector4 color = mt->GetColor();
            color.w = visible ? 1.0f : 0.0f;
            mt->SetColor(color);
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
    void StartAnimation() {
        isAnimating_ = true;
        isFinished_ = false;
        isFinishedTriggered_ = false;
        elapsedTime_ = 0.0f;
    }

    void Escape() {
        float t = Normalize01(elapsedTime_, easingTranslateStartTime_, easingTranslateEndTime_);

        if (auto *tr = playerModel_->GetComponent3D<Transform3D>()) {
            Vector3 newTranslate = Lerp(fromTranslate_, toTranslate_, t);
            tr->SetTranslate(newTranslate);
        }

        if (elapsedTime_ >= translateDurationTime_) {
            isAnimating_ = false;
            isFinished_ = true;
        }
    }

    Model *playerModel_ = nullptr;

    Vector3 fromTranslate_ = Vector3(0.0f, 0.0f, 16.0f);
    Vector3 toTranslate_ = Vector3(0.0f, 0.0f, 3.0f);

    const float easingTranslateStartTime_ = 0.0f;
    const float easingTranslateEndTime_ = 2.0f;

    const float translateDurationTime_ = 2.0f;
    float elapsedTime_ = 0.0f;

    bool isAnimating_ = false;
    bool isFinished_ = false;
    bool isFinishedTriggered_ = false;
};

} // namespace KashipanEngine
