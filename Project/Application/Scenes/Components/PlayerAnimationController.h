#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/PlayerMovementController.h"

#include <algorithm>

namespace KashipanEngine {

class PlayerAnimationController final : public ISceneComponent {
public:
    explicit PlayerAnimationController(Object3DBase *player)
        : ISceneComponent("PlayerAnimationController", 1), player_(player) {}
    ~PlayerAnimationController() override = default;

    void Initialize() override {
        CachePlayerComponents();
        wasGroundedPrev_ = false;
        hasPlayedFalling_ = false;
        fallDistance_ = 0.0f;
    }

    void Update() override {
        CachePlayerComponents();
        if (!playerMovementController_ || !modelAnimator_) return;

        const bool grounded = playerMovementController_->ConsumeGrounded();
        const Vector3 gravityDir = playerMovementController_->GetGravityDirection().Normalize();
        const float fallSpeed = playerMovementController_->GetGravityVelocity().Dot(gravityDir);
        const bool falling = (!grounded) && (fallSpeed > 0.0f);
        auto *playerTr = player_->GetComponent3D<Transform3D>();
        if (grounded && !wasGroundedPrev_) {
            StopAnimation("Player", "PlayerFalling");
            StopAnimation("Player", "PlayerJump");
            if (!modelAnimator_->IsPlaying("Player", "PlayerRun")) {
                particleManager_->Spawn("PlayerLanding", Vector3(0.0f, 0.0f, 0.0f));
                particleManager_->SetParentTransform("PlayerLanding", playerTr);

                PlayAnimation("Player", "PlayerRun");
            }
        }
        if (playerMovementController_->ConsumeJumpTriggered()) {
            StopAnimation("Player", "PlayerRun");
            PlayAnimation("Player", "PlayerJump");
        }

        UpdateFallingAnimation(falling, fallSpeed);

        wasGroundedPrev_ = grounded;
        if (!falling) {
            fallDistance_ = 0.0f;
            hasPlayedFalling_ = false;
        }
    }

private:
    void CachePlayerComponents() {
        if (!player_) {
            auto *ctx = GetOwnerContext();
            player_ = ctx ? ctx->GetObject3D("PlayerRoot") : nullptr;
        }
        if (player_ && !playerMovementController_) {
            playerMovementController_ = player_->GetComponent3D<PlayerMovementController>();
        }
        if (!modelAnimator_) {
            auto *ctx = GetOwnerContext();
            modelAnimator_ = ctx ? ctx->GetComponent<ModelAnimator>() : nullptr;
        }
        if (!particleManager_) {
            auto *ctx = GetOwnerContext();
            particleManager_ = ctx ? ctx->GetComponent<ParticleManager>() : nullptr;
        }
    }

    void PlayAnimation(const char *objectPreset, const char *bindingPreset) {
        if (!modelAnimator_) return;
        modelAnimator_->Play(objectPreset, bindingPreset);
    }

    void StopAnimation(const char *objectPreset, const char *bindingPreset) {
        if (!modelAnimator_) return;
        modelAnimator_->Stop(objectPreset, bindingPreset);
    }

    void UpdateFallingAnimation(bool falling, float fallSpeed) {
        if (!falling) return;
        fallDistance_ += std::max(0.0f, fallSpeed * std::max(0.0f, GetDeltaTime() * GetGameSpeed()));
        if (hasPlayedFalling_) return;
        if (fallDistance_ < fallStartDistance_) return;
        StopAnimation("Player", "PlayerRun");
        PlayAnimation("Player", "PlayerFalling");
        hasPlayedFalling_ = true;
    }

    Object3DBase *player_ = nullptr;
    PlayerMovementController *playerMovementController_ = nullptr;
    ModelAnimator *modelAnimator_ = nullptr;
    ParticleManager *particleManager_ = nullptr;

    bool wasGroundedPrev_ = false;
    bool hasPlayedFalling_ = false;

    float fallDistance_ = 0.0f;
    float fallStartDistance_ = 4.0f;
};

} // namespace KashipanEngine
