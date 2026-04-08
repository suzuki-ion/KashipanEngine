#pragma once

#include <KashipanEngine.h>
#include "Scenes/Components/StageGroundGenerator.h"
#include "Objects/Components/PlayerMovementController.h"

#include <algorithm>

namespace KashipanEngine {

class GameSceneUIController final : public ISceneComponent {
public:
    GameSceneUIController() : ISceneComponent("GameSceneUIController", 1) {}
    ~GameSceneUIController() override = default;

    void Initialize() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        auto *defaultVars = ctx->GetComponent<SceneDefaultVariables>();
        if (!defaultVars) return;

        auto *screenBuffer2D = defaultVars->GetScreenBuffer2D();
        if (!screenBuffer2D) return;

        auto speedBar = std::make_unique<SpriteProressBar>();
        speedBar->SetName("ForwardSpeedBar");
        speedBar->SetBarSize(Vector2{512.0f, 32.0f});
        speedBar->SetFrameThickness(8.0f);
        speedBar->SetFrameColor(Vector4{0.5f, 0.5f, 0.5f, 1.0f});
        speedBar->SetBackgroundColor(Vector4{0.1f, 0.1f, 0.1f, 1.0f});
        speedBar->SetBarColor(Vector4{0.0f, 0.5f, 0.0f, 1.0f});
        speedBar->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = speedBar->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{320.0f, 32.0f, 0.0f});
        }
        forwardSpeedBar_ = speedBar.get();
        (void)ctx->AddObject2D(std::move(speedBar));

        auto gravityBar = std::make_unique<SpriteProressBar>();
        gravityBar->SetName("GravityGaugeBar");
        gravityBar->SetBarSize(Vector2{512.0f, 20.0f});
        gravityBar->SetFrameThickness(6.0f);
        gravityBar->SetFrameColor(Vector4{0.35f, 0.35f, 0.55f, 1.0f});
        gravityBar->SetBackgroundColor(Vector4{0.08f, 0.08f, 0.1f, 1.0f});
        gravityBar->SetBarColor(Vector4{0.2f, 0.6f, 1.0f, 1.0f});
        gravityBar->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = gravityBar->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{320.0f, 70.0f, 0.0f});
        }
        gravityGaugeBar_ = gravityBar.get();
        (void)ctx->AddObject2D(std::move(gravityBar));

        auto speedText = std::make_unique<Text>(128);
        speedText->SetName("ForwardSpeedText");
        speedText->SetFont("Assets/Application/Image/test.fnt");
        speedText->SetTextFormat("Speed: {0:.2f}", 0.0f);
        speedText->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        forwardSpeedText_ = speedText.get();
        (void)ctx->AddObject2D(std::move(speedText));

        auto touchedGroundText = std::make_unique<Text>(128);
        touchedGroundText->SetName("TouchedGroundCountText");
        touchedGroundText->SetFont("Assets/Application/Image/test.fnt");
        touchedGroundText->SetTextFormat("Touched Ground: {0}", 0);
        touchedGroundText->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = touchedGroundText->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{32.0f, 110.0f, 0.0f});
        }
        touchedGroundCountText_ = touchedGroundText.get();
        (void)ctx->AddObject2D(std::move(touchedGroundText));

        auto fallDistanceText = std::make_unique<Text>(128);
        fallDistanceText->SetName("FallDistanceText");
        fallDistanceText->SetFont("Assets/Application/Image/test.fnt");
        fallDistanceText->SetTextFormat("Fall Distance: {0:.2f}", 0.0f);
        fallDistanceText->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = fallDistanceText->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{32.0f, 150.0f, 0.0f});
        }
        fallDistanceText_ = fallDistanceText.get();
        (void)ctx->AddObject2D(std::move(fallDistanceText));
    }

    void Update() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        if (!player_) {
            player_ = ctx->GetObject3D("Player");
        }
        if (player_ && !playerMovementController_) {
            playerMovementController_ = player_->GetComponent3D<PlayerMovementController>();
        }
        if (!stageGroundGenerator_) {
            stageGroundGenerator_ = ctx->GetComponent<StageGroundGenerator>();
        }

        if (forwardSpeedBar_ && playerMovementController_) {
            const float speed = playerMovementController_->GetForwardSpeed();
            const float minSpeed = playerMovementController_->GetMinForwardSpeed();
            const float maxSpeed = playerMovementController_->GetMaxForwardSpeed();
            const float range = std::max(0.0001f, maxSpeed - minSpeed);
            const float progress = std::clamp((speed - minSpeed) / range, 0.0f, 1.0f);
            forwardSpeedBar_->SetProgress(progress);
            if (forwardSpeedText_) {
                forwardSpeedText_->SetTextFormat("Speed: {0:.2f}", speed);
            }
        }

        if (gravityGaugeBar_ && playerMovementController_) {
            gravityGaugeBar_->SetProgress(playerMovementController_->GetGravityGaugeNormalized());
        }

        if (touchedGroundCountText_) {
            const int touchedCount = stageGroundGenerator_ ? stageGroundGenerator_->GetTouchedGroundCount() : 0;
            touchedGroundCountText_->SetTextFormat("Touched Ground: {0}", touchedCount);
        }

        if (fallDistanceText_ && playerMovementController_) {
            fallDistanceText_->SetTextFormat("Fall Distance: {0:.2f}", playerMovementController_->GetAccumulatedFallDistance());
        }
    }

private:
    Object3DBase *player_ = nullptr;
    PlayerMovementController *playerMovementController_ = nullptr;
    StageGroundGenerator *stageGroundGenerator_ = nullptr;

    SpriteProressBar *forwardSpeedBar_ = nullptr;
    SpriteProressBar *gravityGaugeBar_ = nullptr;
    Text *forwardSpeedText_ = nullptr;
    Text *touchedGroundCountText_ = nullptr;
    Text *fallDistanceText_ = nullptr;
};

} // namespace KashipanEngine
