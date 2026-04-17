#pragma once
#pragma once

#include <KashipanEngine.h>

#include "Scenes/GameScene.h"
#include "Scenes/Components/GameOverUIController.h"
#include "Scenes/Components/GameClearUIController.h"
#include "Scenes/Components/PauseUIController.h"
#include "Objects/Components/PlayerInputHandler.h"
#include "Objects/Components/PlayerMovementController.h"

#include <algorithm>
#include <cmath>

namespace KashipanEngine {

class GameSceneAudioPlayer final : public ISceneComponent {
public:
    GameSceneAudioPlayer(GameScene *gameScene, Object3DBase *player)
        : ISceneComponent("GameSceneAudioPlayer", 1),
          gameScene_(gameScene),
          player_(player) {}

    ~GameSceneAudioPlayer() override = default;

    void Initialize() override {
        bgmSoundHandle_ = AudioManager::GetSoundHandleFromFileName("bgmGameScene.mp3");
        jumpSeSoundHandle_ = AudioManager::GetSoundHandleFromFileName("seJump.mp3");
        softLandingSeSoundHandle_ = AudioManager::GetSoundHandleFromFileName("seSoftLanding.mp3");
        hardLandingSeSoundHandle_ = AudioManager::GetSoundHandleFromFileName("seHardLanding.mp3");
        gravityModeEnterSeSoundHandle_ = AudioManager::GetSoundHandleFromFileName("seGravityModeEnter.mp3");
        gravityChangedSeSoundHandle_ = AudioManager::GetSoundHandleFromFileName("seGravityChanged.mp3");
        gravityGaugeLackSeSoundHandle_ = AudioManager::GetSoundHandleFromFileName("seGravityGaugeLack.mp3");
        gameOverSeSoundHandle_ = AudioManager::GetSoundHandleFromFileName("seGameOver.mp3");
        clearSeSoundHandle_ = AudioManager::GetSoundHandleFromFileName("seGameClear.mp3");
        gameOverUiSelectSeSoundHandle_ = AudioManager::GetSoundHandleFromFileName("seUISelect.mp3");
        gameOverUiSubmitSeSoundHandle_ = AudioManager::GetSoundHandleFromFileName("seUISubmit.mp3");
        gameClearUiSelectSeSoundHandle_ = AudioManager::GetSoundHandleFromFileName("seUISelect.mp3");
        gameClearUiSubmitSeSoundHandle_ = AudioManager::GetSoundHandleFromFileName("seUISubmit.mp3");
        pauseUiSelectSeSoundHandle_ = AudioManager::GetSoundHandleFromFileName("seUISelect.mp3");
        pauseUiSubmitSeSoundHandle_ = AudioManager::GetSoundHandleFromFileName("seUISubmit.mp3");

        if (bgmSoundHandle_ != AudioManager::kInvalidSoundHandle) {
            bgmPlayHandle_ = AudioManager::Play(bgmSoundHandle_, 1.0f, 0.0f, true);
        }

        prevGameOver_ = gameScene_ ? gameScene_->IsGameOver() : false;
        prevCleared_ = gameScene_ ? gameScene_->IsCleared() : false;
    }

    void Finalize() override {
        if (bgmPlayHandle_ != AudioManager::kInvalidPlayHandle) {
            (void)AudioManager::Stop(bgmPlayHandle_);
            bgmPlayHandle_ = AudioManager::kInvalidPlayHandle;
        }
    }

    void Update() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        if (!player_) {
            player_ = ctx->GetObject3D("PlayerRoot");
        }
        if (!player_) return;

        if (!playerMovement_) {
            playerMovement_ = player_->GetComponent3D<PlayerMovementController>();
        }
        if (!playerInputHandler_) {
            playerInputHandler_ = player_->GetComponent3D<PlayerInputHandler>();
        }

        auto *inputCommand = ctx->GetInputCommand();

        if (!gameOverUIController_) {
            gameOverUIController_ = ctx->GetComponent<GameOverUIController>();
        }
        if (!gameClearUIController_) {
            gameClearUIController_ = ctx->GetComponent<GameClearUIController>();
        }
        if (!pauseUIController_) {
            pauseUIController_ = ctx->GetComponent<PauseUIController>();
        }

        if (inputCommand) {
            const bool selectTriggered = inputCommand->Evaluate("SelectUp").Triggered() || inputCommand->Evaluate("SelectDown").Triggered();
            const bool submitTriggered = inputCommand->Evaluate("Submit").Triggered();

            if (gameOverUIController_ && gameOverUIController_->IsActive()) {
                if (selectTriggered) {
                    PlaySE(gameOverUiSelectSeSoundHandle_);
                }
                if (submitTriggered) {
                    PlaySE(gameOverUiSubmitSeSoundHandle_);
                }
            } else if (gameClearUIController_ && gameClearUIController_->IsActive()) {
                if (selectTriggered) {
                    PlaySE(gameClearUiSelectSeSoundHandle_);
                }
                if (submitTriggered) {
                    PlaySE(gameClearUiSubmitSeSoundHandle_);
                }
            } else if (pauseUIController_ && pauseUIController_->IsActive()) {
                if (selectTriggered) {
                    PlaySE(pauseUiSelectSeSoundHandle_);
                }
                if (submitTriggered) {
                    PlaySE(pauseUiSubmitSeSoundHandle_);
                }
            }
        }

        if (inputCommand && playerMovement_) {
            const bool isAnyBlockingUiActive =
                (gameOverUIController_ && gameOverUIController_->IsActive()) ||
                (gameClearUIController_ && gameClearUIController_->IsActive()) ||
                (pauseUIController_ && pauseUIController_->IsActive());
            const bool canOperatePlayer =
                !playerMovement_->IsMovementLocked() &&
                (!gameScene_ || (!gameScene_->IsGameOver() && !gameScene_->IsCleared())) &&
                !isAnyBlockingUiActive;

            const bool jumpPressed = inputCommand->Evaluate("PlayerJump").Triggered();
            const bool jumpPressedThisFrame = jumpPressed && !prevJumpPressed_;
            const bool canJumpNow = !playerMovement_->IsMovementLocked()
                && (!playerInputHandler_ || !playerInputHandler_->IsGravitySwitching())
                && (playerMovement_->GetJumpCount() < playerMovement_->GetMaxJumpCount());
            if (jumpPressedThisFrame && canJumpNow) {
                PlaySE(jumpSeSoundHandle_);
            }
            prevJumpPressed_ = jumpPressed;

            if (inputCommand->Evaluate("PlayerGravitySwitchTrigger").Triggered() && canOperatePlayer && !playerMovement_->CanUseGravityChange()) {
                PlaySE(gravityGaugeLackSeSoundHandle_);
            }
        } else {
            prevJumpPressed_ = false;
        }

        if (playerInputHandler_) {
            const bool gravitySwitching = playerInputHandler_->IsGravitySwitching();
            if (gravitySwitching && !prevGravitySwitching_) {
                PlaySE(gravityModeEnterSeSoundHandle_);
            }
            if (playerInputHandler_->ConsumeGravityChangedByInputEvent()) {
                PlaySE(gravityChangedSeSoundHandle_);
            }
            prevGravitySwitching_ = gravitySwitching;
        }

        if (playerMovement_) {
            const bool grounded = playerMovement_->ConsumeGrounded();
            if (!grounded) {
                airborneDurationSec_ += std::max(0.0f, GetDeltaTime());
                maxAirborneFallDistance_ = std::max(maxAirborneFallDistance_, playerMovement_->GetAccumulatedFallDistance());
            }

            landingSeCooldownSec_ = std::max(0.0f, landingSeCooldownSec_ - std::max(0.0f, GetDeltaTime()));
            if (grounded && !wasGroundedPrevFrame_ && landingSeCooldownSec_ <= 0.0f && airborneDurationSec_ >= minAirborneDurationForLandingSe_) {
                const float landingImpact = maxAirborneFallDistance_;
                const float t = std::clamp((landingImpact - landingImpactThreshold_) /
                        std::max(0.0001f, landingImpactForMaxShake_ - landingImpactThreshold_),
                    0.0f,
                    1.0f);
                const float hardVolume = std::pow(t, 0.6f);
                const float softVolume = 1.0f - hardVolume;
                PlaySE(softLandingSeSoundHandle_, softVolume);
                PlaySE(hardLandingSeSoundHandle_, hardVolume);
                landingSeCooldownSec_ = landingSeCooldownDurationSec_;
            }

            if (grounded) {
                airborneDurationSec_ = 0.0f;
                maxAirborneFallDistance_ = 0.0f;
            }
            wasGroundedPrevFrame_ = grounded;

            const Vector3 currentGravityDirection = playerMovement_->GetGravityDirection();
            prevGravityDirection_ = currentGravityDirection;
            hasPrevGravityDirection_ = true;
        }

        if (gameScene_) {
            const bool currentGameOver = gameScene_->IsGameOver();
            if (currentGameOver && !prevGameOver_) {
                PlaySE(gameOverSeSoundHandle_);
            }
            prevGameOver_ = currentGameOver;

            const bool currentCleared = gameScene_->IsCleared();
            if (currentCleared && !prevCleared_) {
                PlaySE(clearSeSoundHandle_);
            }
            prevCleared_ = currentCleared;
        }
    }

private:
    static void PlaySE(AudioManager::SoundHandle handle, float volume = 1.0f) {
        if (handle == AudioManager::kInvalidSoundHandle) return;
        (void)AudioManager::Play(handle, std::clamp(volume, 0.0f, 1.0f), 0.0f, false);
    }

private:
    GameScene *gameScene_ = nullptr;
    Object3DBase *player_ = nullptr;
    PlayerMovementController *playerMovement_ = nullptr;
    PlayerInputHandler *playerInputHandler_ = nullptr;
    GameOverUIController *gameOverUIController_ = nullptr;
    GameClearUIController *gameClearUIController_ = nullptr;
    PauseUIController *pauseUIController_ = nullptr;

    AudioManager::SoundHandle bgmSoundHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle jumpSeSoundHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle softLandingSeSoundHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle hardLandingSeSoundHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle gravityModeEnterSeSoundHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle gravityChangedSeSoundHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle gravityGaugeLackSeSoundHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle gameOverSeSoundHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle clearSeSoundHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle gameOverUiSelectSeSoundHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle gameOverUiSubmitSeSoundHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle gameClearUiSelectSeSoundHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle gameClearUiSubmitSeSoundHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle pauseUiSelectSeSoundHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle pauseUiSubmitSeSoundHandle_ = AudioManager::kInvalidSoundHandle;

    AudioManager::PlayHandle bgmPlayHandle_ = AudioManager::kInvalidPlayHandle;

    bool prevGravitySwitching_ = false;
    bool prevJumpPressed_ = false;
    bool prevGameOver_ = false;
    bool prevCleared_ = false;

    bool hasPrevGravityDirection_ = false;
    Vector3 prevGravityDirection_{0.0f, 1.0f, 0.0f};

    float landingImpactThreshold_ = 6.0f;
    float landingImpactForMaxShake_ = 64.0f;
    bool wasGroundedPrevFrame_ = true;
    float maxAirborneFallDistance_ = 0.0f;
    float airborneDurationSec_ = 0.0f;
    float landingSeCooldownSec_ = 0.0f;
    float landingSeCooldownDurationSec_ = 0.12f;
    float minAirborneDurationForLandingSe_ = 0.03f;
};

} // namespace KashipanEngine
