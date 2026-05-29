#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class PlayerInputHandler : public IObjectComponent3D {
public:
    PlayerInputHandler(InputCommand *inputCommand)
        : IObjectComponent3D("PlayerInputHandler", 1), inputCommand_(inputCommand) {}

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<PlayerInputHandler>(*this);
    }


private:
    InputCommand *inputCommand_ = nullptr;
};

} // namespace KashipanEngine