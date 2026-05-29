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

#ifdef USE_IMGUI
    void ShowImGui() override {
        ImGui::Text("PlayerInputHandler Component");
    }
#endif

private:
};

} // namespace KashipanEngine