#pragma once
#include <KashipanEngine.h>

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
        return true;
    }

    bool IsMoveLeft() const {
        return inputCommand_ && inputCommand_->Evaluate("PlayerMoveLeft").Triggered();
    }
    bool IsMoveRight() const {
        return inputCommand_ && inputCommand_->Evaluate("PlayerMoveRight").Triggered();
    }
    bool IsMoveUp() const {
        return inputCommand_ && inputCommand_->Evaluate("PlayerMoveUp").Triggered();
    }
    bool IsMoveDown() const {
        return inputCommand_ && inputCommand_->Evaluate("PlayerMoveDown").Triggered();
    }

#ifdef USE_IMGUI
    void ShowImGui() override {
        ImGui::Text("PlayerInputHandler Component");
    }
#endif

private:
    InputCommand *inputCommand_ = nullptr;
};

} // namespace KashipanEngine