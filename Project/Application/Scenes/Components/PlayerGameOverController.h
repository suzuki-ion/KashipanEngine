#pragma once

#include <KashipanEngine.h>
#include "Scenes/GameScene.h"
#include "Scenes/Components/StageNoiseWallController.h"
#include "Scenes/Components/GameOverUIController.h"
#include "Objects/Components/PlayerMovementController.h"

#include <algorithm>

namespace KashipanEngine {

class PlayerGameOverController final : public ISceneComponent {
public:
    PlayerGameOverController(GameScene *gameScene, Object3DBase *player)
        : ISceneComponent("PlayerGameOverController", 1),
          gameScene_(gameScene),
          player_(player) {}

    ~PlayerGameOverController() override = default;

    void Initialize() override {
        CacheComponents();
        danger_ = 0.0f;
    }

    float GetDanger() const { return danger_; }

    void Update() override {
        if (!gameScene_ || !gameScene_->IsPlaying()) {
            danger_ = 0.0f;
            return;
        }

        CacheComponents();
        if (!player_) {
            danger_ = 0.0f;
            return;
        }

        auto *playerTr = player_->GetComponent3D<Transform3D>();
        if (!playerTr) {
            danger_ = 0.0f;
            return;
        }

        const Vector3 playerPos = playerTr->GetTranslate();
        danger_ = ComputeWallDanger(playerPos);

        if (IsBehindNoiseWall(playerPos)) {
            StartGameOver();
        }
    }

private:
    void CacheComponents() {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        if (!player_) {
            player_ = ctx->GetObject3D("PlayerRoot");
        }
        if (!player_) return;

        if (!playerMovementController_) {
            playerMovementController_ = player_->GetComponent3D<PlayerMovementController>();
        }
        if (!noiseWallController_) {
            noiseWallController_ = ctx->GetComponent<StageNoiseWallController>();
        }
        if (!gameOverUIController_) {
            gameOverUIController_ = ctx->GetComponent<GameOverUIController>();
        }
        if (!particleManager_) {
            particleManager_ = ctx->GetComponent<ParticleManager>();
        }
    }

    float ComputeWallDanger(const Vector3 &playerPos) const {
        if (!noiseWallController_) return 0.0f;
        const float wallZ = noiseWallController_->GetWallPositionZ();
        const float dz = playerPos.z - wallZ;
        return std::clamp(1.0f - ((-dz) / std::max(0.0001f, wallDangerDistance_)), 0.0f, 1.0f);
    }

    bool IsBehindNoiseWall(const Vector3 &playerPos) const {
        if (!noiseWallController_) return false;
        const float wallZ = noiseWallController_->GetWallPositionZ();
        const float dz = playerPos.z - wallZ;
        return dz >= 0.0f;
    }

    void StartGameOver() {
        if (gameScene_) {
            gameScene_->SetGameOverState();
        }
        if (playerMovementController_) {
            playerMovementController_->SetMovementLocked(true);
        }
        if (noiseWallController_) {
            noiseWallController_->SetMovementEnabled(false);
        }
        HidePlayerVisuals();

        if (gameOverUIController_ && !gameOverUIController_->IsActive()) {
            gameOverUIController_->Activate();
        }

        if (particleManager_ && player_) {
            auto *playerTr = player_->GetComponent3D<Transform3D>();
            particleManager_->SetParentTransform("PlayerDeath", playerTr);
            particleManager_->Spawn("PlayerDeath", Vector3{0.0f, 0.0f, 0.0f});
        }
    }

    void HidePlayerVisuals() {
        static constexpr const char *kPlayerParts[] = { "PlayerBody", "PlayerHead", "PlayerArmL", "PlayerArmR" };
        for (const char *name : kPlayerParts) {
            auto *obj = GetOwnerContext() ? GetOwnerContext()->GetObject3D(name) : nullptr;
            if (!obj) continue;
            auto *mat = obj->GetComponent3D<Material3D>();
            if (!mat) continue;
            auto c = mat->GetColor();
            c.w = 0.0f;
            mat->SetColor(c);
        }
    }

private:
    GameScene *gameScene_ = nullptr;
    Object3DBase *player_ = nullptr;
    PlayerMovementController *playerMovementController_ = nullptr;
    StageNoiseWallController *noiseWallController_ = nullptr;
    GameOverUIController *gameOverUIController_ = nullptr;
    ParticleManager *particleManager_ = nullptr;

    float wallDangerDistance_ = 32.0f;
    float danger_ = 0.0f;
};

} // namespace KashipanEngine
