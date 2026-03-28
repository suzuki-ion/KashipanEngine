#pragma once

#include "Scenes/Components/PuzzleBlockFaller.h"

#include <string>

namespace Application::ScenePuzzle {

class PuzzlePlayer final : public KashipanEngine::ISceneComponent {
public:
    PuzzlePlayer(
        Application::PuzzleBlockFaller *faller,
        const std::string &moveHorizontalCommand,
        const std::string &rotateCommand,
        const std::string &placeCommand)
        : ISceneComponent("PuzzlePlayer", 2),
          faller_(faller),
          moveHorizontalCommand_(moveHorizontalCommand),
          rotateCommand_(rotateCommand),
          placeCommand_(placeCommand) {}

    ~PuzzlePlayer() override = default;

    void Update() override {
        auto *ctx = GetOwnerContext();
        if (!ctx || !faller_) return;
        if (ctx->GetSceneVariableOr<bool>("IsPuzzleStop", true)) return;

        auto *ic = ctx->GetInputCommand();
        if (!ic) return;

        const auto horizontal = ic->Evaluate(moveHorizontalCommand_);
        if (horizontal.Triggered()) {
            if (horizontal.Value() < 0.0f) {
                (void)faller_->MoveLeft();
            } else if (horizontal.Value() > 0.0f) {
                (void)faller_->MoveRight();
            }
        }

        const auto rotation = ic->Evaluate(rotateCommand_);
        if (rotation.Triggered()) {
            if (rotation.Value() < 0.0f) {
                (void)faller_->RotateLeft();
            } else if (rotation.Value() > 0.0f) {
                (void)faller_->RotateRight();
            }
        }

        if (ic->Evaluate(placeCommand_).Triggered()) {
            (void)faller_->PlaceCurrentBlock();
        }
    }

private:
    Application::PuzzleBlockFaller *faller_ = nullptr;

    std::string moveHorizontalCommand_;
    std::string rotateCommand_;
    std::string placeCommand_;
};

} // namespace Application::ScenePuzzle
