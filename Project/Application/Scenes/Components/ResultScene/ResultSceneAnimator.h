#pragma once
#include <KashipanEngine.h>
#include "Scenes/Components/ResultScene/CarEscape.h"
#include "Scenes/Components/ResultScene/PlayerEscape.h"
#include "Scenes/Components/ResultScene/ShowScoreNumModels.h"

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
        showScoreNumModels_ = ctx->GetComponent<ShowScoreNumModels>();
        letterbox_ = ctx->GetComponent<Letterbox>();
        if (letterbox_) {
            letterbox_->SetThickness(0.0f);
        }
        if (playerEscape_) {
            playerEscape_->Reset();
            playerEscape_->SetVisible(true);
        }
        if (carEscape_) {
            carEscape_->Reset();
        }
        if (showScoreNumModels_) {
            showScoreNumModels_->SetVisible(false);
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
                step_ = Step::PlayerMove;
            } else {
                step_ = Step::CarMoveIn;
            }
            started_ = true;
        }

        if (!started_) return;

        switch (step_) {
            case Step::PlayerMove:
                if (playerEscape_ && playerEscape_->IsFinished()) {
                    if (carEscape_) {
                        carEscape_->StartMoveIn();
                        step_ = Step::CarMoveIn;
                    } else {
                        step_ = Step::ShowScore;
                    }
                }
                break;
            case Step::CarMoveIn:
                if (carEscape_ && carEscape_->IsFinished()) {
                    if (playerEscape_) {
                        playerEscape_->SetVisible(false);
                    }
                    if (showScoreNumModels_) {
                        showScoreNumModels_->SetVisible(true);
                    }
                    if (carEscape_) {
                        carEscape_->StartMoveOut();
                        step_ = Step::CarMoveOut;
                    } else {
                        step_ = Step::Finished;
                    }
                }
                break;
            case Step::CarMoveOut:
                if (carEscape_ && carEscape_->IsFinished()) {
                    step_ = Step::Finished;
                }
                break;
            case Step::ShowScore:
                if (playerEscape_) {
                    playerEscape_->SetVisible(false);
                }
                if (showScoreNumModels_) {
                    showScoreNumModels_->SetVisible(true);
                }
                if (carEscape_) {
                    carEscape_->StartMoveOut();
                    step_ = Step::CarMoveOut;
                } else {
                    step_ = Step::Finished;
                }
                break;
            case Step::Finished:
                break;
        }

        if (step_ == Step::Finished) {
            isAnimationFinished_ = true;
            isAnimating_ = false;
        }
    }

    void EndAnimation() {
        if (playerEscape_) {
            playerEscape_->EndAnimation();
            playerEscape_->SetVisible(false);
        }
        if (carEscape_) {
            carEscape_->EndAnimation();
        }
        if (showScoreNumModels_) {
            showScoreNumModels_->SetVisible(true);
        }
        isAnimating_ = false;
        isAnimationFinished_ = true;
        started_ = true;
        elapsedTime_ = startDelaySec_;
        step_ = Step::Finished;
    }

    bool IsAnimating() const {
        return isAnimating_;
    }

    bool IsAnimationFinished() const {
        return isAnimationFinished_;
    }

private:
    enum class Step {
        PlayerMove,
        CarMoveIn,
        ShowScore,
        CarMoveOut,
        Finished
    };

    RegisterFunc registerFunc_;
    InputCommand *inputCommand_ = nullptr;

    CarEscape *carEscape_ = nullptr;
    PlayerEscape *playerEscape_ = nullptr;
    ShowScoreNumModels *showScoreNumModels_ = nullptr;
    Letterbox *letterbox_ = nullptr;

    Step step_ = Step::PlayerMove;

    bool started_ = false;
    bool isAnimating_ = false;
    bool isAnimationFinished_ = false;
    float elapsedTime_ = 0.0f;
    const float startDelaySec_ = 1.0f;
};

} // namespace KashipanEngine
