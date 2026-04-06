#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/PlayerMovementController.h"

#include <cmath>
#include <optional>
#include <string>

namespace KashipanEngine {

class PlayerInputHandler final : public IObjectComponent3D {
public:
    PlayerInputHandler(
        InputCommand *inputCommand,
        std::string moveRightCommand,
        std::string moveLeftCommand,
        std::string jumpCommand,
        std::string gravitySwitchTriggerCommand,
        std::string gravitySwitchReleaseCommand,
        std::string upCommand,
        std::string downCommand,
        std::string leftCommand,
        std::string rightCommand)
        : IObjectComponent3D("PlayerInputHandler", 1),
          inputCommand_(inputCommand),
          moveRightCommand_(std::move(moveRightCommand)),
          moveLeftCommand_(std::move(moveLeftCommand)),
          jumpCommand_(std::move(jumpCommand)),
          gravitySwitchTriggerCommand_(std::move(gravitySwitchTriggerCommand)),
          gravitySwitchReleaseCommand_(std::move(gravitySwitchReleaseCommand)),
          upCommand_(std::move(upCommand)),
          downCommand_(std::move(downCommand)),
          leftCommand_(std::move(leftCommand)),
          rightCommand_(std::move(rightCommand)) {}

    ~PlayerInputHandler() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<PlayerInputHandler>(
            inputCommand_,
            moveRightCommand_,
            moveLeftCommand_,
            jumpCommand_,
            gravitySwitchTriggerCommand_,
            gravitySwitchReleaseCommand_,
            upCommand_,
            downCommand_,
            leftCommand_,
            rightCommand_);
        ptr->isGravitySwitching_ = isGravitySwitching_;
        ptr->requestedGravityDirection_ = requestedGravityDirection_;
        return ptr;
    }

    std::optional<bool> Initialize() override {
        auto *ctx = GetOwner3DContext();
        if (!ctx) return false;

        playerMovement_ = ctx->GetComponent<PlayerMovementController>();
        return playerMovement_ != nullptr;
    }

    std::optional<bool> Update() override {
        if (!inputCommand_ || !playerMovement_) return false;

        if (inputCommand_->Evaluate(gravitySwitchTriggerCommand_).Triggered()) {
            isGravitySwitching_ = true;
            requestedGravityDirection_ = std::nullopt;
            SetGameSpeed(0.2f);
        }

        if (isGravitySwitching_) {
            UpdateGravitySwitchDirection();

            if (inputCommand_->Evaluate(gravitySwitchReleaseCommand_).Triggered()) {
                if (requestedGravityDirection_.has_value()) {
                    playerMovement_->SetGravityDirection(*requestedGravityDirection_);
                }
                isGravitySwitching_ = false;
                requestedGravityDirection_ = std::nullopt;
                SetGameSpeed(1.0f);
            }
            return true;
        }

        const auto right = inputCommand_->Evaluate(moveRightCommand_);
        if (right.Triggered()) {
            playerMovement_->MoveRight(std::abs(right.Value()));
        }

        const auto left = inputCommand_->Evaluate(moveLeftCommand_);
        if (left.Triggered()) {
            playerMovement_->MoveLeft(std::abs(left.Value()));
        }

        if (inputCommand_->Evaluate(jumpCommand_).Triggered()) {
            playerMovement_->Jump();
        }

        return true;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    void UpdateGravitySwitchDirection() {
        const Vector3 down = playerMovement_->GetGravityDirection().Normalize();
        const Vector3 up = -down;

        Vector3 left = down.Cross(playerMovement_->GetForwardDirection());
        if (left.LengthSquared() <= 0.000001f) {
            left = Vector3{1.0f, 0.0f, 0.0f};
        } else {
            left = left.Normalize();
        }

        if (inputCommand_->Evaluate(upCommand_).Triggered()) {
            requestedGravityDirection_ = up;
        } else if (inputCommand_->Evaluate(downCommand_).Triggered()) {
            requestedGravityDirection_ = down;
        } else if (inputCommand_->Evaluate(leftCommand_).Triggered()) {
            requestedGravityDirection_ = left;
        } else if (inputCommand_->Evaluate(rightCommand_).Triggered()) {
            requestedGravityDirection_ = -left;
        }
    }

    InputCommand *inputCommand_ = nullptr;
    PlayerMovementController *playerMovement_ = nullptr;

    std::string moveRightCommand_;
    std::string moveLeftCommand_;
    std::string jumpCommand_;
    std::string gravitySwitchTriggerCommand_;
    std::string gravitySwitchReleaseCommand_;
    std::string upCommand_;
    std::string downCommand_;
    std::string leftCommand_;
    std::string rightCommand_;

    bool isGravitySwitching_ = false;
    std::optional<Vector3> requestedGravityDirection_{};
};

} // namespace KashipanEngine
