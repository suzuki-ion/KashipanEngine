#include "AutoSmoothMoveComponent.h"

namespace KashipanEngine {

std::optional<bool> AutoSmoothMoveComponent::Initialize() {
    auto ownerContext = GetOwner2DContext();
    if (auto *tr = ownerContext->GetComponent<Transform2D>()) {
        startPosition_ = tr->GetTranslate();
        targetPosition_ = startPosition_;
        currentPosition_ = startPosition_;
    }
    return true;
}

std::optional<bool> AutoSmoothMoveComponent::Update() {
    auto ownerContext = GetOwner2DContext();
    if (auto *tr = ownerContext->GetComponent<Transform2D>()) {
        const Vector3 observedPosition = tr->GetTranslate();

        // 外部から座標変更が入ったか判定（移動中に同じ target が毎フレーム書かれてもリセットしない）
        const bool changedFromCurrent = (observedPosition != currentPosition_);
        const bool changedFromTarget = (observedPosition != targetPosition_);

        if (changedFromCurrent && (!isMoving_ || changedFromTarget)) {
            startPosition_ = currentPosition_;
            targetPosition_ = observedPosition;
            elapsedTime_ = 0.0f;
            isMoving_ = true;
        }

        if (isMoving_) {
            if (moveDuration_ <= 0.0f) {
                currentPosition_ = targetPosition_;
                tr->SetTranslate(currentPosition_);
                isMoving_ = false;
                startPosition_ = targetPosition_;
                return true;
            }

            elapsedTime_ += GetDeltaTime();
            const float t = Normalize01(elapsedTime_, 0.0f, moveDuration_);
            currentPosition_ = Eased(startPosition_, targetPosition_, t, easeType_);
            tr->SetTranslate(currentPosition_);

            if (t >= 1.0f) {
                isMoving_ = false;
                startPosition_ = targetPosition_;
                currentPosition_ = targetPosition_;
                tr->SetTranslate(targetPosition_);
            }
        }
    }
    return true;
}

#ifdef USE_IMGUI
void AutoSmoothMoveComponent::ShowImGui() {
    ImGui::Text("Ease Type:");
    const char* easeTypeNames[] = {
        "Linear",
        "EaseInQuad", "EaseOutQuad", "EaseInOutQuad", "EaseOutInQuad",
        "EaseInCubic", "EaseOutCubic", "EaseInOutCubic", "EaseOutInCubic",
        "EaseInQuart", "EaseOutQuart", "EaseInOutQuart", "EaseOutInQuart",
        "EaseInQuint", "EaseOutQuint", "EaseInOutQuint", "EaseOutInQuint",
        "EaseInSine", "EaseOutSine", "EaseInOutSine", "EaseOutInSine",
        "EaseInExpo", "EaseOutExpo", "EaseInOutExpo", "EaseOutInExpo",
        "EaseInCirc", "EaseOutCirc", "EaseInOutCirc", "EaseOutInCirc",
        "EaseInBack", "EaseOutBack", "EaseInOutBack", "EaseOutInBack",
        "EaseInElastic", "EaseOutElastic", "EaseInOutElastic", "EaseOutInElastic",
        "EaseInBounce", "EaseOutBounce", "EaseInOutBounce", "EaseOutInBounce"
    };
    int currentType = static_cast<int>(easeType_);
    if (ImGui::Combo("##easeTypeCombo", &currentType, easeTypeNames, IM_ARRAYSIZE(easeTypeNames))) {
        easeType_ = static_cast<EaseType>(currentType);
    }
    ImGui::InputFloat("Duration", &moveDuration_, 0.1f, 10.0f, "%.2f");
    ImGui::Text("Is Moving: %s", isMoving_ ? "Yes" : "No");
    ImGui::Text("elapsedTime: %.2f / %.2f", elapsedTime_, moveDuration_);
}
#endif

} // namespace KashipanEngine