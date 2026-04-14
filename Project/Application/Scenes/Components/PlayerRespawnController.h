#pragma once

#include <KashipanEngine.h>
#include "Scenes/GameScene.h"
#include "Scenes/Components/StageNoiseWallController.h"
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
        if (playerMovementController_) {
            initialForwardSpeed_ = std::max(0.0f, playerMovementController_->GetForwardSpeed());
        }
    }

    void Update() override {
        if (!gameScene_ || (!gameScene_->IsPlaying() && !isRespawning_)) {
            return;
        }

        CachePlayerComponents();
        if (!player_) return;

        auto *playerTr = player_->GetComponent3D<Transform3D>();
        if (!playerTr) return;

        if (!isRespawning_) {
            const Vector3 playerPos = playerTr->GetTranslate();
            if (ShouldStartRespawn(playerPos)) {
                StartRespawn(playerPos);
            }
            return;
        }

        respawnElapsed_ += std::max(0.0f, GetDeltaTime());
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
            if (playerMovementController_) {
                initialForwardSpeed_ = std::max(0.0f, playerMovementController_->GetForwardSpeed());
            }
        }
        if (!noiseWallController_) {
            noiseWallController_ = GetOwnerContext() ? GetOwnerContext()->GetComponent<StageNoiseWallController>() : nullptr;
        }
        if (!particleManager_) {
            particleManager_ = GetOwnerContext() ? GetOwnerContext()->GetComponent<ParticleManager>() : nullptr;
        }
    }

    bool ShouldStartRespawn(const Vector3 &playerPos) const {
        if (!gameScene_ || !gameScene_->IsPlaying()) return false;

        const float radial = std::sqrt(playerPos.x * playerPos.x + playerPos.y * playerPos.y);
        if (radial >= stageBoundaryRadius_) return true;

        if (noiseWallController_) {
            const float wallZ = noiseWallController_->GetWallPositionZ();
            const float dz = playerPos.z - wallZ;
            if (dz >= 0.0f) return true;
        }

        return false;
    }

    void StartRespawn(const Vector3 &playerPos) {
        isRespawning_ = true;
        respawnElapsed_ = 0.0f;
        respawnZ_ = playerPos.z;

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

        if (auto *wallRoot = GetOwnerContext() ? GetOwnerContext()->GetObject3D("NoiseWallRoot") : nullptr) {
            if (auto *wallTr = wallRoot->GetComponent3D<Transform3D>()) {
                wallTr->SetTranslate(respawnWallPosition_);
            }
        }

        if (playerMovementController_) {
            playerMovementController_->SetGravityDirection(Vector3{0.0f, -1.0f, 0.0f});
            playerMovementController_->SetForwardSpeed(initialForwardSpeed_);
            playerMovementController_->SetLateralVelocity(Vector3{0.0f, 0.0f, 0.0f});
            playerMovementController_->SetGravityVelocity(Vector3{0.0f, 0.0f, 0.0f});
            playerMovementController_->SetMovementLocked(false);
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
    ParticleManager *particleManager_ = nullptr;

    bool isRespawning_ = false;
    float respawnElapsed_ = 0.0f;
    float respawnDelay_ = 2.0f;
    float respawnZ_ = 0.0f;
    float initialForwardSpeed_ = 32.0f;
    Vector3 respawnWallPosition_{0.0f, 0.0f, 256.0f};
    float stageBoundaryRadius_ = 64.0f * 6.0f;
};

} // namespace KashipanEngine
