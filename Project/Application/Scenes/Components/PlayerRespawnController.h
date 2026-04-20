#pragma once

#include <KashipanEngine.h>
#include "Scenes/GameScene.h"
#include "Scenes/Components/StageNoiseWallController.h"
#include "Scenes/Components/StageGroundGenerator.h"
#include "Objects/Components/PlayerMovementController.h"

#include <algorithm>

namespace KashipanEngine {

class PlayerRespawnController final : public ISceneComponent {
public:
    PlayerRespawnController(GameScene *gameScene, Object3DBase *player)
        : ISceneComponent("PlayerRespawnController", 1),
          gameScene_(gameScene),
          player_(player) {}

    ~PlayerRespawnController() override = default;

    void Initialize() override {
        CachePlayerComponents();
        danger_ = 0.0f;
    }

    bool IsRespawning() const { return isRespawning_; }
    float GetDanger() const { return danger_; }

    void Update() override {
        if (!gameScene_ || (!gameScene_->IsPlaying() && !isRespawning_)) {
            danger_ = 0.0f;
            return;
        }

        CachePlayerComponents();
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
        danger_ = ComputeDanger(playerPos);

        if (!isRespawning_) {
            if (ShouldStartRespawn(playerPos)) {
                StartRespawn(playerPos);
            }
            return;
        }

        respawnElapsed_ += std::max(0.0f, GetDeltaTime() * GetGameSpeed());
        if (respawnElapsed_ >= respawnDelay_) {
            FinishRespawn();
        }
    }

private:
    void CachePlayerComponents() {
        if (!player_) {
            auto *ctx = GetOwnerContext();
            if (ctx) {
                player_ = ctx->GetObject3D("PlayerRoot");
            }
        }
        if (!player_) return;

        if (!playerMovementController_) {
            playerMovementController_ = player_->GetComponent3D<PlayerMovementController>();
        }
        if (!noiseWallController_) {
            noiseWallController_ = GetOwnerContext() ? GetOwnerContext()->GetComponent<StageNoiseWallController>() : nullptr;
        }
        if (!stageGroundGenerator_) {
            stageGroundGenerator_ = GetOwnerContext() ? GetOwnerContext()->GetComponent<StageGroundGenerator>() : nullptr;
        }
        if (!particleManager_) {
            particleManager_ = GetOwnerContext() ? GetOwnerContext()->GetComponent<ParticleManager>() : nullptr;
        }
    }

    float ComputeDanger(const Vector3 &playerPos) const {
        const float radial = std::sqrt(playerPos.x * playerPos.x + playerPos.y * playerPos.y);
        const float boundaryStart = stageBoundaryRadius_ * stageBoundaryDangerStartRatio_;
        return std::clamp((radial - boundaryStart) / std::max(0.0001f, stageBoundaryRadius_ - boundaryStart), 0.0f, 1.0f);
    }

    bool ShouldStartRespawn(const Vector3 &playerPos) const {
        if (!gameScene_ || !gameScene_->IsPlaying()) return false;
        if (isRespawning_) return false;

        const float radial = std::sqrt(playerPos.x * playerPos.x + playerPos.y * playerPos.y);
        return radial >= stageBoundaryRadius_;
    }

    void StartRespawn(const Vector3 &playerPos) {
        isRespawning_ = true;
        respawnElapsed_ = 0.0f;
        respawnZ_ = playerPos.z;

        if (playerMovementController_) {
            playerMovementController_->SetMovementLocked(true);
        }
        HidePlayerVisuals();

        if (particleManager_ && player_) {
            auto *playerTr = player_->GetComponent3D<Transform3D>();
            particleManager_->SetParentTransform("PlayerDeath", playerTr);
            particleManager_->Spawn("PlayerDeath", Vector3{0.0f, 0.0f, 0.0f});
        }
    }

    void FinishRespawn() {
        if (!player_) return;

        auto *playerTr = player_->GetComponent3D<Transform3D>();
        if (playerTr) {
            playerTr->SetTranslate(Vector3{0.0f, 0.0f, respawnZ_});
        }

        if (playerMovementController_) {
            playerMovementController_->SetGravityDirection(Vector3{0.0f, -1.0f, 0.0f});
            playerMovementController_->SetForwardSpeed(initialForwardSpeed_);
            playerMovementController_->SetLateralVelocity(Vector3{0.0f, 0.0f, 0.0f});
            playerMovementController_->SetGravityVelocity(Vector3{0.0f, 0.0f, 0.0f});
            playerMovementController_->SetMovementLocked(false);
        }
        if (stageGroundGenerator_ && playerTr) {
            stageGroundGenerator_->RespawnStartGroundUnderPlayer();
        }
        if (noiseWallController_) {
            noiseWallController_->SetMovementEnabled(true);
        }
        ShowPlayerVisuals();

        isRespawning_ = false;
        respawnElapsed_ = 0.0f;
        if (gameScene_) {
            gameScene_->SetPlayingState();
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

    void ShowPlayerVisuals() {
        static constexpr const char *kPlayerParts[] = { "PlayerBody", "PlayerHead", "PlayerArmL", "PlayerArmR" };
        for (const char *name : kPlayerParts) {
            auto *obj = GetOwnerContext() ? GetOwnerContext()->GetObject3D(name) : nullptr;
            if (!obj) continue;
            auto *mat = obj->GetComponent3D<Material3D>();
            if (!mat) continue;
            auto c = mat->GetColor();
            c.w = 1.0f;
            mat->SetColor(c);
        }
    }

private:
    GameScene *gameScene_ = nullptr;
    Object3DBase *player_ = nullptr;
    PlayerMovementController *playerMovementController_ = nullptr;
    StageNoiseWallController *noiseWallController_ = nullptr;
    StageGroundGenerator *stageGroundGenerator_ = nullptr;
    ParticleManager *particleManager_ = nullptr;

    bool isRespawning_ = false;
    float respawnElapsed_ = 0.0f;
    float respawnDelay_ = 1.0f;
    float respawnZ_ = 0.0f;
    float initialForwardSpeed_ = 64.0f;
    float stageBoundaryRadius_ = 32.0f * 6.0f;
    float stageBoundaryDangerStartRatio_ = 0.7f;
    float danger_ = 0.0f;
};

} // namespace KashipanEngine
