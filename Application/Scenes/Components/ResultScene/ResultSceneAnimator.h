#pragma once
#include <KashipanEngine.h>
#include "Scenes/Components/TitleScene/CameraStartMovement.h"
#include "Scenes/Components/ResultScene/CarEscape.h"
#include "Scenes/Components/ResultScene/PlayerEscape.h"

namespace KashipanEngine {

class ResultSceneAnimator final : public ISceneComponent {
public:
    using RegisterFunc = std::function<bool(std::unique_ptr<ISceneComponent>)>;

    ResultSceneAnimator(RegisterFunc regFunc, InputCommand *ic)
        : ISceneComponent("ResultSceneAnimator"), registerFunc_(regFunc), inputCommand_(ic) {
        if (registerFunc_) {
            registerFunc_(std::make_unique<CarEscape>());
            registerFunc_(std::make_unique<PlayerEscape>());
        }
    }

    void Initialize() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;
        carEscape_ = ctx->GetComponent<CarEscape>();
        playerEscape_ = ctx->GetComponent<PlayerEscape>();
        cameraMovement_ = ctx->GetComponent<CameraStartMovement>();
        letterbox_ = ctx->GetComponent<Letterbox>();
        if (letterbox_) {
            letterbox_->SetThickness(0.0f);
        }
    }

    void Finalize() override {
    }

    void Update() override {
        if (isAnimationFinished_) return;

        elapsedTime_ += GetDeltaTime();

        if (!started_ && elapsedTime_ >= startDelaySec_) {
            if (playerEscape_) {
                playerEscape_->StartEscape();
                isAnimating_ = true;
            }
            started_ = true;
        }

        if (playerEscape_) {
            if (playerEscape_->IsFinishedTriggered()) {
                if (playerEscape_->IsPlayerEscape()) {
                    playerEscape_->StartDisappear();
                    isAnimating_ = true;
                } else {
                    if (carEscape_) {
                        carEscape_->StartMoveOut();
                        isAnimating_ = true;
                    }
                    if (cameraMovement_) {
                        cameraMovement_->StartAnimation();
                        isAnimating_ = true;
                    }
                }
            }
        }

        if (carEscape_ && cameraMovement_) {
            if (carEscape_->IsFinished() && cameraMovement_->IsFinished()) {
                isAnimationFinished_ = true;
                isAnimating_ = false;
            }
        } else if (carEscape_) {
            if (carEscape_->IsFinished()) {
                isAnimationFinished_ = true;
                isAnimating_ = false;
            }
        }
    }

    void EndAnimation() {
        if (playerEscape_) {
            playerEscape_->EndAnimation();
        }
        if (carEscape_) {
            carEscape_->EndAnimation();
        }
        if (cameraMovement_) {
            cameraMovement_->EndAnimation();
        }
        isAnimating_ = false;
        isAnimationFinished_ = true;
        started_ = true;
        elapsedTime_ = startDelaySec_;
    }

    bool IsAnimating() const {
        return isAnimating_;
    }

    bool IsAnimationFinished() const {
        return isAnimationFinished_;
    }

private:
    RegisterFunc registerFunc_;
    InputCommand *inputCommand_ = nullptr;

    CarEscape *carEscape_ = nullptr;
    PlayerEscape *playerEscape_ = nullptr;
    CameraStartMovement *cameraMovement_ = nullptr;
    Letterbox *letterbox_ = nullptr;

    bool started_ = false;
    bool isAnimating_ = false;
    bool isAnimationFinished_ = false;
    float elapsedTime_ = 0.0f;
    const float startDelaySec_ = 1.0f;
};

} // namespace KashipanEngine
