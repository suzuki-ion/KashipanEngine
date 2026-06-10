#pragma once
#include <KashipanEngine.h>
#include "PlayerInputHandler.h"
#include "PlayerCollisionPushBack.h"

namespace KashipanEngine {

class PlayerMovement : public IObjectComponent3D {
public:
    PlayerMovement()
        : IObjectComponent3D("PlayerMovement", 1) {}

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<PlayerMovement>();
    }

    std::optional<bool> Initialize() override {
        playerInputHandler_ = GetOwner3DContext()->GetComponent<PlayerInputHandler>();
        if (!playerInputHandler_) return false;
        playerCollisionPushBack_ = GetOwner3DContext()->GetComponent<PlayerCollisionPushBack>();
        if (!playerCollisionPushBack_) return false;
        return true;
    }

    std::optional<bool> Update() override {
        if (!playerInputHandler_) return false;
        if (!playerCollisionPushBack_) return false;
        auto *transform = GetOwner3DContext()->GetComponent<Transform3D>();
        if (!transform) return false;
        const float dt = GetDeltaTime() * GetGameSpeed();

        if (playerCollisionPushBack_->IsGrounded()) {
            isJumping_ = false;
            velocity_.y = 0.0f;
        }

        if (playerInputHandler_->IsMoveLeft()) {
            velocity_.x += -moveSpeed_;
        } else if (playerInputHandler_->IsMoveRight()) {
            velocity_.x += moveSpeed_;
        } else {
            velocity_.x = std::lerp(velocity_.x, 0.0f, lateralDeceleration_);
        }
        if (!isJumping_ && playerInputHandler_->IsMoveUp()) {
            velocity_.y = jumpPower_;
            isJumping_ = true;
        }

        // 重力の適用
        velocity_.y -= gravity_;

        // 速度の適用
        velocity_.x = std::clamp(velocity_.x, minVelocity_.x, maxVelocity_.x);
        velocity_.y = std::clamp(velocity_.y, minVelocity_.y, maxVelocity_.y);
        Vector3 newPos = transform->GetTranslate() + velocity_ * dt;
        transform->SetTranslate(newPos);

        return true;
    }

#ifdef USE_IMGUI
    void ShowImGui() override {
        ImGui::Text("PlayerMovement Component");
    }
#endif

private:
    PlayerInputHandler *playerInputHandler_ = nullptr;
    PlayerCollisionPushBack *playerCollisionPushBack_ = nullptr;
    Vector3 velocity_ = Vector3::Zero();
    Vector3 minVelocity_ = Vector3(-8.0f, -16.0f, -8.0f);
    Vector3 maxVelocity_ = Vector3(8.0f, 16.0f, 8.0f);

    float moveSpeed_ = 0.1f;
    float jumpPower_ = 16.0f;
    float gravity_ = 0.2f;
    float lateralDeceleration_ = 0.1f;

    bool isJumping_ = false;
};

} // namespace KashipanEngine