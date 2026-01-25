#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class PlayerEnter final : public ISceneComponent {
public:
    PlayerEnter() : ISceneComponent("PlayerEnter") {}

    void Initialize() override {
        auto *ownerContext = GetOwnerContext();
        auto *sceneDefaultVariables = ownerContext->GetComponent<SceneDefaultVariables>();
        auto *screenBuffer3D = sceneDefaultVariables->GetScreenBuffer3D();
        auto *shadowMapBuffer = sceneDefaultVariables->GetShadowMapBuffer();

        // プレイヤーモデル
        auto modelHandle = ModelManager::GetModelDataFromFileName("player.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetName("PlayerModel");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(fromTranslate_);
            tr->SetScale(fromScale_);
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
        Appear();
        Enter();
    }

    void StartAppearance() {
        if (isAnimating_) return;
        isPlayerAppearance_ = true;
        StartAnimation();
    }

    void StartEnter() {
        if (isAnimating_) return;
        isPlayerAppearance_ = false;
        StartAnimation();
    }

    void Reset() {
        if (playerModel_) {
            if (auto *tr = playerModel_->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(fromTranslate_);
                tr->SetScale(fromScale_);
            }
            if (auto *mt = playerModel_->GetComponent3D<Material3D>()) {
                Vector4 color = mt->GetColor();
                color.w = 1.0f;
                mt->SetColor(color);
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
    bool IsPlayerAppearance() const { return isPlayerAppearance_; }

private:
    void StartAnimation() {
        isAnimating_ = true;
        isFinished_ = false;
        isFinishedTriggered_ = false;
        elapsedTime_ = 0.0f;
    }

    void Appear() {
        if (!isPlayerAppearance_) return;
        float t = Normalize01(elapsedTime_, easingScaleStartTime_, easingScaleEndTime_);
        
        if (auto *tr = playerModel_->GetComponent3D<Transform3D>()) {
            Vector3 newScale = EaseOutElastic(fromScale_, toScale_, t);
            tr->SetScale(newScale);
        }

        if (elapsedTime_ >= scaleDurationTime_) {
            isAnimating_ = false;
            isFinished_ = true;
        }
    }
    void Enter() {
        if (isPlayerAppearance_) return;
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

    Vector3 fromTranslate_ = Vector3(0.0f, 0.0f, 2.0f);
    Vector3 fromScale_ = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 toTranslate_ = Vector3(0.0f, 0.0f, 16.0f);
    Vector3 toScale_ = Vector3(1.0f, 1.0f, 1.0f);
    
    const float easingTranslateStartTime_ = 0.0f;
    const float easingTranslateEndTime_ = 2.0f;
    
    const float easingScaleStartTime_ = 0.5f;
    const float easingScaleEndTime_ = 2.0f;

    const float translateDurationTime_ = 2.0f;
    const float scaleDurationTime_ = 2.0f;
    float elapsedTime_ = 0.0f;

    bool isAnimating_ = false;
    bool isFinished_ = false;
    bool isFinishedTriggered_ = false;
    bool isPlayerAppearance_ = false;
};

} // namespace KashipanEngine
