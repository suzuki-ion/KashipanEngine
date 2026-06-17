#pragma once
#include <KashipanEngine.h>
#include "PlayerMovement.h"

namespace KashipanEngine {

class PlayerInputHandler : public IObjectComponent3D {
public:
    PlayerInputHandler()
        : IObjectComponent3D("PlayerInputHandler", 1) {}

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<PlayerInputHandler>();
    }

    std::optional<bool> Initialize() override {
        inputCommand_ = GetOwnerSceneContext()->GetInputCommand();
        if (!inputCommand_) return false;
        return true;
    }

    std::optional<bool> Update() override {
        if (!isActive_) return true;
        if (!playerMovement_) {
            playerMovement_ = GetOwner3DContext()->GetComponent<PlayerMovement>();
        }
        if (!playerMovement_ || !inputCommand_) return false;

        auto moveLeftInfo = inputCommand_->Evaluate("PlayerMoveLeft");
        if (moveLeftInfo.Triggered()) {
            playerMovement_->MoveLeft(true, moveLeftInfo.Value());
        } else {
            playerMovement_->MoveLeft(false, 0.0f);
        }

        auto moveRightInfo = inputCommand_->Evaluate("PlayerMoveRight");
        if (moveRightInfo.Triggered()) {
            playerMovement_->MoveRight(true, moveRightInfo.Value());
        } else {
            playerMovement_->MoveRight(false, 0.0f);
        }

        auto jumpInfo = inputCommand_->Evaluate("PlayerJump");
        if (jumpInfo.Triggered()) {
            playerMovement_->Jump(true);
        } else {
            playerMovement_->Jump(false);
        }

        return true;
    }

#ifdef USE_IMGUI
    void ShowImGui() override {
        ImGui::Text("PlayerInputHandler Component");
    }
#endif

    void SetActive(bool active) { isActive_ = active; }
    bool IsActive() const { return isActive_; }

private:
    PlayerMovement *playerMovement_ = nullptr;
    InputCommand *inputCommand_ = nullptr;

    bool isActive_ = true;
};

REGISTER_COMPONENT_OBJECT3D(PlayerInputHandler)

} // namespace KashipanEngine