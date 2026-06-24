#pragma once
#include <KashipanEngine.h>
#include "PlayerCollision.h"

namespace KashipanEngine {

class PlayerMovement : public IObjectComponent3D {
public:
    PlayerMovement()
        : IObjectComponent3D("PlayerMovement", 1) {}

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<PlayerMovement>();
    }

    std::optional<bool> Update() override {
        playerCollision_ = GetOwner3DContext()->GetComponent<PlayerCollision>();
        if (!playerCollision_) return false;
        auto *transform = GetOwner3DContext()->GetComponent<Transform3D>();
        if (!transform) return false;
        const float dt = GetDeltaTime() * GetGameSpeed();
        const bool isGrounded = playerCollision_->IsGrounded();
        const bool isCollidingWithEnemy = playerCollision_->IsCollidingWithEnemy();
        const Vector3 &hitNormal = playerCollision_->GetHitNormal();
        const Vector3 targetNormal = isGrounded ? Vector3(hitNormal.y, -hitNormal.x, 0.0f).Normalize() : Vector3(1.0f, 0.0f, 0.0f);

        // 左右移動
        if (isMoveLeft_) {
            velocity_ -= targetNormal * moveSpeed_ * moveLeftInput_;
        } else if (isMoveRight_) {
            velocity_ += targetNormal * moveSpeed_ * moveRightInput_;
        } else {
            velocity_.x = std::lerp(velocity_.x, 0.0f, lateralDeceleration_);
            if (isGrounded && !isJumping_ && !wasJumping_) {
                velocity_.y = std::lerp(velocity_.y, 0.0f, lateralDeceleration_);
            }
        }

        // 重力とジャンプ
        velocity_.y = (isGrounded || isCollidingWithEnemy) ? velocity_.y : velocity_.y - gravity_;
        if ((isGrounded || isCollidingWithEnemy) && isJumping_ && !wasJumping_) {
            velocity_.y = jumpPower_;
        }
        wasJumping_ = isJumping_;

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

    void MoveLeft(bool isPressed, float input) { isMoveLeft_ = isPressed; moveLeftInput_ = input; }
    void MoveRight(bool isPressed, float input) { isMoveRight_ = isPressed; moveRightInput_ = input; }
    void Jump(bool isPressed) { isJumping_ = isPressed; }

private:
    PlayerCollision *playerCollision_ = nullptr;
    Vector3 velocity_ = Vector3::Zero();
    Vector3 minVelocity_ = Vector3(-8.0f, -16.0f, -8.0f);
    Vector3 maxVelocity_ = Vector3(8.0f, 16.0f, 8.0f);

    float moveSpeed_ = 0.1f;
    float jumpPower_ = 16.0f;
    float gravity_ = 0.2f;
    float lateralDeceleration_ = 0.1f;

    bool isJumping_ = false;
    bool wasJumping_ = false;
    bool isMoveLeft_ = false;
    bool isMoveRight_ = false;
    float moveLeftInput_ = 0.0f;
    float moveRightInput_ = 0.0f;
};

REGISTER_COMPONENT_OBJECT3D(PlayerMovement)

} // namespace KashipanEngine