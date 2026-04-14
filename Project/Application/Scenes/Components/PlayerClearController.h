#pragma once

#include <KashipanEngine.h>
#include "Scenes/GameScene.h"
#include "Scenes/Components/CameraToPlayerSync.h"
#include "Scenes/Components/GameClearUIController.h"
#include "Scenes/Components/StageGoalPlaneController.h"
#include "Scenes/Components/StageGroundGenerator.h"
#include "Scenes/Components/StageNoiseWallController.h"
#include "Objects/Components/PlayerMovementController.h"

#include <algorithm>

namespace KashipanEngine {

class PlayerClearController final : public ISceneComponent {
public:
    PlayerClearController(GameScene *gameScene, Object3DBase *player)
        : ISceneComponent("PlayerClearController", 1),
          gameScene_(gameScene),
          player_(player) {}

    ~PlayerClearController() override = default;

    void Update() override {
        if (!gameScene_ || (!gameScene_->IsPlaying() && !clearActive_)) return;

        CacheComponents();
        if (!player_ || !playerMovementController_ || !goalPlaneController_) return;

        auto *playerTr = player_->GetComponent3D<Transform3D>();
        if (!playerTr) return;

        const Vector3 playerPos = playerTr->GetTranslate();
        if (!clearActive_ && playerPos.z < goalPlaneController_->GetGoalZ()) {
            StartClear();
        }

        if (!clearActive_) return;

        clearElapsed_ += std::max(0.0f, GetDeltaTime());
        const float t = std::clamp(clearElapsed_ / std::max(0.0001f, clearDuration_), 0.0f, 1.0f);
        const float inv = 1.0f - t;

        playerMovementController_->SetForwardSpeed(clearStartForwardSpeed_ * inv);
        playerMovementController_->SetLateralVelocity(clearStartLateralVelocity_ * inv);
        playerMovementController_->SetGravityVelocity(clearStartGravityVelocity_ * inv);

        if (t >= 1.0f) {
            playerMovementController_->SetForwardSpeed(0.0f);
            playerMovementController_->SetLateralVelocity(Vector3{0.0f, 0.0f, 0.0f});
            playerMovementController_->SetGravityVelocity(Vector3{0.0f, 0.0f, 0.0f});
            playerMovementController_->SetMovementLocked(true);
            clearActive_ = false;
        }
    }

private:
    void CacheComponents() {
        if (!player_) {
            player_ = GetOwnerContext() ? GetOwnerContext()->GetObject3D("PlayerRoot") : nullptr;
        }
        if (!player_) return;

        if (!playerMovementController_) {
            playerMovementController_ = player_->GetComponent3D<PlayerMovementController>();
        }
        if (!goalPlaneController_) {
            goalPlaneController_ = GetOwnerContext() ? GetOwnerContext()->GetComponent<StageGoalPlaneController>() : nullptr;
        }
        if (!noiseWallController_) {
            noiseWallController_ = GetOwnerContext() ? GetOwnerContext()->GetComponent<StageNoiseWallController>() : nullptr;
        }
        if (!gameClearUIController_) {
            gameClearUIController_ = GetOwnerContext() ? GetOwnerContext()->GetComponent<GameClearUIController>() : nullptr;
        }
        if (!cameraToPlayerSync_) {
            cameraToPlayerSync_ = GetOwnerContext() ? GetOwnerContext()->GetComponent<CameraToPlayerSync>() : nullptr;
        }
        if (!stageGroundGenerator_) {
            stageGroundGenerator_ = GetOwnerContext() ? GetOwnerContext()->GetComponent<StageGroundGenerator>() : nullptr;
        }
    }

    void StartClear() {
        clearActive_ = true;
        clearElapsed_ = 0.0f;
        if (gameScene_) {
            gameScene_->SetClearedState();
        }
        if (playerMovementController_) {
            clearStartForwardSpeed_ = std::max(0.0f, playerMovementController_->GetForwardSpeed());
            clearStartLateralVelocity_ = playerMovementController_->GetLateralVelocity();
            clearStartGravityVelocity_ = playerMovementController_->GetGravityVelocity();
        }
        if (noiseWallController_) {
            noiseWallController_->SetMovementEnabled(false);
        }
        if (cameraToPlayerSync_) {
            cameraToPlayerSync_->SetClearViewEnabled(true);
        }
        if (gameClearUIController_) {
            const int touched = stageGroundGenerator_ ? stageGroundGenerator_->GetTouchedGroundCount() : 0;
            if (!gameClearUIController_->IsActive()) {
                gameClearUIController_->Activate(touched);
            }
        }
    }

private:
    GameScene *gameScene_ = nullptr;
    Object3DBase *player_ = nullptr;
    PlayerMovementController *playerMovementController_ = nullptr;
    StageGoalPlaneController *goalPlaneController_ = nullptr;
    StageNoiseWallController *noiseWallController_ = nullptr;
    GameClearUIController *gameClearUIController_ = nullptr;
    CameraToPlayerSync *cameraToPlayerSync_ = nullptr;
    StageGroundGenerator *stageGroundGenerator_ = nullptr;

    bool clearActive_ = false;
    float clearElapsed_ = 0.0f;
    float clearDuration_ = 1.0f;
    float clearStartForwardSpeed_ = 0.0f;
    Vector3 clearStartLateralVelocity_{0.0f, 0.0f, 0.0f};
    Vector3 clearStartGravityVelocity_{0.0f, 0.0f, 0.0f};
};

} // namespace KashipanEngine
