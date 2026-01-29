#pragma once
#include <KashipanEngine.h>
#include "Scenes/Components/TitleScene/CarMove.h"
#include "Scenes/Components/TitleScene/PlayerEnter.h"
#include "Scenes/Components/TitleScene/StartTextUpdate.h"
#include "Scenes/Components/TitleScene/CameraStartMovement.h"

namespace KashipanEngine {

class TitleSceneAnimator final : public ISceneComponent {
public:
    using RegisterFunc = std::function<bool(std::unique_ptr<ISceneComponent>)>;

    TitleSceneAnimator(RegisterFunc regFunc, InputCommand *ic)
        : ISceneComponent("TitleSceneAnimator"), registerFunc_(regFunc), inputCommand_(ic) {
        if (registerFunc_) {
            registerFunc_(std::make_unique<CameraStartMovement>());
            registerFunc_(std::make_unique<CarMove>());
            registerFunc_(std::make_unique<PlayerEnter>());
            registerFunc_(std::make_unique<StartTextUpdate>(inputCommand_));
        }
    }

    void Initialize() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;
        cameraMovement_ = ctx->GetComponent<CameraStartMovement>();
        carMove_ = ctx->GetComponent<CarMove>();
        playerEnter_ = ctx->GetComponent<PlayerEnter>();
        startTextUpdate_ = ctx->GetComponent<StartTextUpdate>();
    }

    void Finalize() override {}

    void Update() override {
        isAnimating_ = false;
        if (!inputCommand_) return;

#if defined(DEBUG_BUILD)
        if (inputCommand_->Evaluate("DebugReset").Triggered()) {
            if (carMove_) carMove_->Reset();
            if (playerEnter_) playerEnter_->Reset();
            if (startTextUpdate_) startTextUpdate_->Reset();
            if (cameraMovement_) cameraMovement_->Reset();
            return;
        }
#endif

        if (startTextUpdate_) {
            if (startTextUpdate_->IsFinishedTriggered()) {
                if (carMove_) carMove_->StartMoveIn();
                isAnimating_ = true;
            }
        }

        if (carMove_) {
            if (carMove_->IsFinishedTriggered()) {
                if (carMove_->IsMoveIn()) {
                    if (playerEnter_) playerEnter_->StartAppearance();
                    carMove_->StartMoveOut();
                    isAnimating_ = true;
                }
            }
        }

        if (playerEnter_) {
            if (playerEnter_->IsFinishedTriggered()) {
                if (playerEnter_->IsPlayerAppearance()) {
                    if (cameraMovement_) cameraMovement_->StartAnimation();
                    playerEnter_->StartEnter();
                    isAnimating_ = true;
                }
            }
        }

        if (cameraMovement_) {
            if (cameraMovement_->IsAnimating()) {
                isAnimating_ = true;
            }
        }
    }

    bool IsAnimating() const {
        return isAnimating_;
    }

    bool IsAnimationFinished() const {
        if (cameraMovement_) {
            return cameraMovement_->IsFinished();
        }
        return false;
    }
    bool IsAnimationFinishedTriggered() const {
        if (cameraMovement_) {
            return cameraMovement_->IsFinishedTriggered();
        }
        return false;
    }

private:
    RegisterFunc registerFunc_;
    InputCommand *inputCommand_ = nullptr;

    CarMove *carMove_ = nullptr;
    PlayerEnter *playerEnter_ = nullptr;
    StartTextUpdate *startTextUpdate_ = nullptr;
    CameraStartMovement *cameraMovement_ = nullptr;

    bool isAnimating_ = false;
};

} // namespace KashipanEngine
