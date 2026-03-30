#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class AutoSmoothMoveComponent final : public IObjectComponent2D {
public:
    AutoSmoothMoveComponent() : IObjectComponent2D("AutoSmoothMoveComponent", 1) {}
    ~AutoSmoothMoveComponent() override = default;
    
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<AutoSmoothMoveComponent>();
        ptr->startPosition_ = startPosition_;
        ptr->targetPosition_ = targetPosition_;
        ptr->currentPosition_ = currentPosition_;
        ptr->easeType_ = easeType_;
        ptr->moveDuration_ = moveDuration_;
        ptr->elapsedTime_ = elapsedTime_;
        ptr->isMoving_ = isMoving_;
        return ptr;
    }

    std::optional<bool> Initialize() override;
    std::optional<bool> Update() override;

#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif
    bool IsMoving() const { return isMoving_; }

    void SetDuration(float duration);

private:
    Vector3 startPosition_;
    Vector3 targetPosition_;
    Vector3 currentPosition_;
    EaseType easeType_ = EaseType::EaseInOutCirc;
    float moveDuration_ = 0.2f;
    float elapsedTime_ = 0.0f;
    bool isMoving_ = false;
};

} // namespace KashipanEngine