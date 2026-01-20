#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class CameraStartMovement final : public ISceneComponent {
public:
    CameraStartMovement() : ISceneComponent("CameraStartMovement") {}

    void Initialize() override {
    }

    void Finalize() override {
    }

    void Update() override {
    }

    void StartAnimation() {
        isAnimating_ = true;
    }

    bool IsAnimating() const { return isAnimating_; }

private:

    bool isAnimating_ = false;
};

} // namespace KashipanEngine
