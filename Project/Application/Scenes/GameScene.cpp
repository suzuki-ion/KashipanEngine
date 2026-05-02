#include "Scenes/GameScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Scenes/Components/CameraController.h"
#include "Scenes/Components/CameraToPlayerSync.h"
#include "Scenes/Components/PostEffectToPlayerSync.h"
#include "Scenes/Components/GameSceneUIController.h"
#include "Scenes/Components/GameOverUIController.h"
#include "Scenes/Components/GameClearUIController.h"
#include "Scenes/Components/PauseUIController.h"
#include "Scenes/Components/ClearScoreBoard.h"
#include "Scenes/Components/ClearTimeBoard.h"
#include "Scenes/Components/StageGroundGenerator.h"
#include "Scenes/Components/StageNoiseWallController.h"
#include "Scenes/Components/StageGoalPlaneController.h"
#include "Scenes/Components/StageDecoPlaneGenerator.h"
#include "Scenes/Components/StageDecoBoxGenerator.h"
#include "Scenes/Components/StageObjectController.h"
#include "Scenes/Components/StageObjectRandomGenerator.h"
#include "Scenes/Components/GameSceneAudioPlayer.h"
#include "Scenes/Components/PlayerRespawnController.h"
#include "Scenes/Components/PlayerGameOverController.h"
#include "Scenes/Components/PlayerClearController.h"

#include "Objects/GameObjects/3D/Box.h"
#include "Objects/Components/PlayerMovementController.h"
#include "Objects/Components/PlayerInputHandler.h"

#include <algorithm>

namespace KashipanEngine {

GameScene::GameScene()
    : SceneBase("GameScene") {}

void GameScene::Initialize() {
    SetGameSpeed(1.0f);
    wasPlayerGroundedPrevFrame_ = false;
    isPlayerRunParticleActive_ = false;
    particleManager_ = nullptr;
    groundSpawnLimitConfigured_ = false;
    clearSlowdownActive_ = false;
    clearSlowdownElapsed_ = 0.0f;
    clearSlowdownStartForwardSpeed_ = 0.0f;
    clearSlowdownStartLateralVelocity_ = Vector3{0.0f, 0.0f, 0.0f};
    clearSlowdownStartGravityVelocity_ = Vector3{0.0f, 0.0f, 0.0f};

    RadialBlurEffect *radialBlurEffect = nullptr;
    VignetteEffect *vignetteEffect = nullptr;

    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();
    auto *screenBuffer3D = sceneDefaultVariables_->GetScreenBuffer3D();
    auto *directionalLight = sceneDefaultVariables_->GetDirectionalLight();
    directionalLight->SetDirection({ 0.0f, 0.0f, 1.0f });

    if (screenBuffer3D) {
        BloomEffect::Params bp;
        bp.intensity = 1.0f;
        bp.blurRadius = 2.0f;
        bp.iterations = 4;
        bp.softKnee = 0.2f;
        bp.threshold = 0.5f;
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<BloomEffect>(bp));

        RadialBlurEffect::Params rp;
        rp.intensity = 0.0f;
        rp.sampleCount = 16;
        rp.radialCenter[0] = 0.5f;
        rp.radialCenter[1] = 0.5f;
        rp.startRadius = 0.05f;
        auto radialBlur = std::make_unique<RadialBlurEffect>(rp);
        radialBlurEffect = radialBlur.get();
        screenBuffer3D->RegisterPostEffectComponent(std::move(radialBlur));

        VignetteEffect::Params vp;
        vp.center[0] = 0.5f;
        vp.center[1] = 0.5f;
        vp.color = Vector4{0.0f, 0.25f, 0.0f, 1.0f};
        vp.intensity = 0.0f;
        vp.innerRadius = 0.3f;
        vp.smoothness = 0.2f;
        auto vignette = std::make_unique<VignetteEffect>(vp);
        vignetteEffect = vignette.get();
        vignetteEffect_ = vignetteEffect;
        screenBuffer3D->RegisterPostEffectComponent(std::move(vignette));

        screenBuffer3D->AttachToRenderer("ScreenBuffer3D.Default");
    }

    if (sceneDefaultVariables_) {
        if (auto *mainCamera = sceneDefaultVariables_->GetMainCamera3D()) {
            AddSceneComponent(std::make_unique<CameraController>(mainCamera));
        }

        AddSceneComponent(std::make_unique<StageGroundGenerator>());
        AddSceneComponent(std::make_unique<StageDecoPlaneGenerator>());
        AddSceneComponent(std::make_unique<StageDecoBoxGenerator>());
        AddSceneComponent(std::make_unique<StageObjectController>());
        AddSceneComponent(std::make_unique<StageObjectRandomGenerator>());
        AddSceneComponent(std::make_unique<StageNoiseWallController>());
        AddSceneComponent(std::make_unique<StageGoalPlaneController>());
        AddSceneComponent(std::make_unique<GameSceneUIController>());
        AddSceneComponent(std::make_unique<ClearScoreBoard>());
        AddSceneComponent(std::make_unique<GameOverUIController>());
        AddSceneComponent(std::make_unique<GameClearUIController>());
        AddSceneComponent(std::make_unique<PauseUIController>());

        stageGroundGenerator_ = GetSceneComponent<StageGroundGenerator>();
        noiseWallController_ = GetSceneComponent<StageNoiseWallController>();
        goalPlaneController_ = GetSceneComponent<StageGoalPlaneController>();
        gameSceneUIController_ = GetSceneComponent<GameSceneUIController>();
        gameOverUIController_ = GetSceneComponent<GameOverUIController>();
        gameClearUIController_ = GetSceneComponent<GameClearUIController>();
        pauseUIController_ = GetSceneComponent<PauseUIController>();

        if (auto *colliderComp = sceneDefaultVariables_->GetColliderComp()) {
            auto player = std::make_unique<Box>();
            player->SetName("PlayerRoot");
            player->SetUniqueBatchKey();

            Transform3D *playerRootTr = nullptr;

            if (screenBuffer3D) {
                player->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            }

            if (auto *tr = player->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(playerSpawnPosition_);
                playerRootTr = tr;
            }
            if (auto *mat = player->GetComponent3D<Material3D>()) {
                mat->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
            }

            player->RegisterComponent<PlayerMovementController>(colliderComp->GetCollider());
            player->RegisterComponent<PlayerInputHandler>(
                GetInputCommand(),
                "PlayerMoveRight",
                "PlayerMoveLeft",
                "PlayerJump",
                "PlayerForwardSpeedDown",
                "CameraRearConfirm",
                "PlayerGravitySwitchTrigger",
                "PlayerGravitySwitchRelease",
                "PlayerGravityUp",
                "PlayerGravityDown",
                "PlayerGravityLeft",
                "PlayerGravityRight");

            Object3DBase *playerPtr = player.get();
            AddObject3D(std::move(player));
            player_ = playerPtr;
            playerMovementController_ = playerPtr->GetComponent3D<PlayerMovementController>();

            auto addPlayerModel = [&](const char *objectName, const char *modelFileName) {
                auto modelHandle = ModelManager::GetModelHandleFromFileName(modelFileName);
                auto obj = std::make_unique<Model>(modelHandle);
                obj->SetName(objectName);
                if (screenBuffer3D) {
                    obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
                }
                if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                    if (playerRootTr) {
                        tr->SetParentTransform(playerRootTr);
                    }
                    tr->SetTranslate(Vector3{0.0f, 0.0f, 0.0f});
                    tr->SetScale(Vector3{1.0f, 1.0f, 1.0f});
                }
                if (auto *mat = obj->GetComponent3D<Material3D>()) {
                    mat->SetEnableLighting(false);
                    mat->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
                }
                AddObject3D(std::move(obj));
            };

            addPlayerModel("PlayerBody", "float_Body.obj");
            addPlayerModel("PlayerHead", "float_Head.obj");
            addPlayerModel("PlayerArmL", "float_L_arm.obj");
            addPlayerModel("PlayerArmR", "float_R_arm.obj");

            AddSceneComponent(std::make_unique<ModelAnimator>());
            if (auto *modelAnimator = GetSceneComponent<ModelAnimator>()) {
                modelAnimator->LoadFromJsonFile("PlayerAnimation.json");
                modelAnimator->Play("Player", "PlayerRun");
            }

            AddSceneComponent(std::make_unique<GameSceneAudioPlayer>(this, playerPtr));
            AddSceneComponent(std::make_unique<PlayerRespawnController>(this, playerPtr));
            AddSceneComponent(std::make_unique<PlayerGameOverController>(this, playerPtr));
            AddSceneComponent(std::make_unique<PlayerClearController>(this, playerPtr));

            playerRespawnController_ = GetSceneComponent<PlayerRespawnController>();
            playerGameOverController_ = GetSceneComponent<PlayerGameOverController>();

            AddSceneComponent(std::make_unique<CameraToPlayerSync>(playerPtr));
            AddSceneComponent(std::make_unique<PostEffectToPlayerSync>(playerPtr, radialBlurEffect, vignetteEffect));
        }
    }

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());
    AddSceneComponent(std::make_unique<ParticleManager>());

    particleManager_ = GetSceneComponent<ParticleManager>();
    if (particleManager_) {
        particleManager_->LoadFromJsonFile("PlayerParticle.json");
    }

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }

    AddSceneComponent(std::make_unique<ClearTimeBoard>());
    clearTimeBoard_ = GetSceneComponent<ClearTimeBoard>();
    if (clearTimeBoard_) {
        clearTimeBoard_->ResetMeasurement();
        clearTimeBoard_->StartMeasurement();
    }
}

GameScene::~GameScene() {}

void GameScene::OnUpdate() {
    if (!player_) {
        player_ = GetObject3D("PlayerRoot");
    }
    if (player_ && !playerMovementController_) {
        playerMovementController_ = player_->GetComponent3D<PlayerMovementController>();
    }
    if (!particleManager_) {
        particleManager_ = GetSceneComponent<ParticleManager>();
    }
    if (!playerRespawnController_) {
        playerRespawnController_ = GetSceneComponent<PlayerRespawnController>();
    }
    if (!playerGameOverController_) {
        playerGameOverController_ = GetSceneComponent<PlayerGameOverController>();
    }
    if (!clearTimeBoard_) {
        clearTimeBoard_ = GetSceneComponent<ClearTimeBoard>();
    }

    if (!groundSpawnLimitConfigured_ && stageGroundGenerator_ && goalPlaneController_) {
        groundSpawnLimitConfigured_ = true;
    }

    auto queueSceneChange = [&](const std::string &sceneName) {
        if (!GetNextSceneName().empty()) return;
        SetGameSpeed(1.0f);
        SetNextSceneName(sceneName);
    };

    const bool modalVisible = (gameOverUIController_ && gameOverUIController_->IsActive())
        || (gameClearUIController_ && gameClearUIController_->IsActive())
        || (pauseUIController_ && pauseUIController_->IsActive());
    if (gameSceneUIController_) {
        gameSceneUIController_->SetVisible(!modalVisible);
    }

    if (clearTimeBoard_) {
        const bool shouldMeasure = IsPlaying() && !modalVisible;
        if (shouldMeasure) {
            clearTimeBoard_->StartMeasurement();
        } else {
            clearTimeBoard_->PauseMeasurement();
        }
    }

    if (auto *ic = GetInputCommand()) {
        const bool canPause = IsPlaying() || (playerRespawnController_ && playerRespawnController_->IsRespawning());
        if (canPause && !modalVisible && ic->Evaluate("Pause").Triggered()) {
            if (pauseUIController_) {
                pauseUIController_->Activate();
            }
        }
    }

    if (vignetteEffect_) {
        auto v = vignetteEffect_->GetParams();
        const Vector4 red{1.0f, 0.0f, 0.0f, 1.0f};
        float danger = playerRespawnController_ ? playerRespawnController_->GetDanger() : 0.0f;
        if (playerGameOverController_) {
            danger = std::max(danger, playerGameOverController_->GetDanger());
        }
        if (IsGameOver()) {
            danger = std::max(danger, 1.0f);
        }
        v.color = Vector4::Lerp(baseVignetteColor_, red, std::clamp(danger, 0.0f, 1.0f));
        v.intensity = std::max(v.intensity, std::clamp(danger, 0.0f, 1.0f) * 0.8f);
        if (IsGameOver()) {
            v.intensity = std::max(v.intensity, 0.8f);
            v.color = red;
        }
        vignetteEffect_->SetParams(v);
    }

    if (particleManager_ && player_) {
        auto *playerTr = player_->GetComponent3D<Transform3D>();
        const Vector3 playerPos = playerTr ? playerTr->GetTranslate() : Vector3{0.0f, 0.0f, 0.0f};

        if (playerMovementController_) {
            const bool grounded = playerMovementController_->ConsumeGrounded();
            const bool landedThisFrame = (grounded && !wasPlayerGroundedPrevFrame_);
            if (landedThisFrame) {
                particleManager_->SetParentTransform("PlayerLanding", playerTr);
                particleManager_->Spawn("PlayerLanding", Vector3(0.0f, 0.0f, 0.0f));

                particleManager_->Spawn("PlayerRun", playerPos);
                particleManager_->SetEmitting("PlayerRun", true);
                isPlayerRunParticleActive_ = true;
            }

            if (isPlayerRunParticleActive_ && grounded) {
                particleManager_->SetEmitCenter("PlayerRun", playerPos);
            }
            if (isPlayerRunParticleActive_ && !grounded) {
                particleManager_->SetEmitting("PlayerRun", false);
                isPlayerRunParticleActive_ = false;
            }

            wasPlayerGroundedPrevFrame_ = grounded;
        }
    }

    if (gameOverUIController_) {
        const auto action = gameOverUIController_->ConsumeRequestedAction();
        if (action == GameOverUIController::RequestAction::Retry) {
            queueSceneChange("GameScene");
        } else if (action == GameOverUIController::RequestAction::BackToTitle) {
            queueSceneChange("TitleScene");
        }
    }

    if (gameClearUIController_) {
        const auto action = gameClearUIController_->ConsumeRequestedAction();
        if (action == GameClearUIController::RequestAction::Retry) {
            queueSceneChange("GameScene");
        } else if (action == GameClearUIController::RequestAction::BackToTitle) {
            queueSceneChange("TitleScene");
        }
    }

    if (pauseUIController_) {
        const auto action = pauseUIController_->ConsumeRequestedAction();
        if (action == PauseUIController::RequestAction::Continue) {
            pauseUIController_->Deactivate();
        } else if (action == PauseUIController::RequestAction::Retry) {
            queueSceneChange("GameScene");
        } else if (action == PauseUIController::RequestAction::BackToTitle) {
            queueSceneChange("TitleScene");
        }
    }

    if (!GetNextSceneName().empty()) {
        if (auto *out = GetSceneComponent<SceneChangeOut>()) {
            if (out->IsFinished()) {
                ChangeToNextScene();
            }
        }
    }

    if (auto *ic = GetInputCommand()) {
#if defined(DEBUG_BUILD) or defined(DEVELOPMENT_BUILD)
        if (ic->Evaluate("DebugSceneChange").Triggered()) {
            queueSceneChange("TitleScene");
            if (auto *out = GetSceneComponent<SceneChangeOut>()) {
                out->Play();
            }
        }
#endif
    }
}

} // namespace KashipanEngine
