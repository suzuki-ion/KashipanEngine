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
#include "Scenes/Components/StageGroundGenerator.h"
#include "Scenes/Components/StageNoiseWallController.h"
#include "Scenes/Components/StageGoalPlaneController.h"
#include "Scenes/Components/StageDecoPlaneGenerator.h"
#include "Scenes/Components/StageDecoBoxGenerator.h"
#include "Scenes/Components/StageObjectController.h"
#include "Scenes/Components/StageObjectRandomGenerator.h"
#include "Scenes/Components/GameSceneAudioPlayer.h"

#include "Objects/GameObjects/3D/Box.h"
#include "Objects/Components/PlayerMovementController.h"
#include "Objects/Components/PlayerInputHandler.h"

#include <algorithm>

namespace KashipanEngine {

GameScene::GameScene()
    : SceneBase("GameScene") {}

void GameScene::Initialize() {
    SetGameSpeed(1.0f);
    wasGameOverPrevFrame_ = false;
    wasPlayerGroundedPrevFrame_ = false;
    isPlayerRunParticleActive_ = false;
    particleManager_ = nullptr;
    groundSpawnLimitConfigured_ = false;
    clearSlowdownActive_ = false;
    clearSlowdownElapsed_ = 0.0f;
    clearSlowdownStartGameSpeed_ = 1.0f;

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
        rp.startRadius = 0.1f;
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
                tr->SetTranslate(Vector3{0.0f, 0.0f, -2.0f});
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

    if (!groundSpawnLimitConfigured_ && stageGroundGenerator_ && goalPlaneController_) {
        stageGroundGenerator_->SetMinSpawnZ(goalPlaneController_->GetGoalZ());
        groundSpawnLimitConfigured_ = true;
    }

    auto hidePlayerVisuals = [&]() {
        if (!player_) return;
        if (auto *mat = player_->GetComponent3D<Material3D>()) {
            auto c = mat->GetColor();
            c.w = 0.0f;
            mat->SetColor(c);
        }
        static constexpr const char *kPlayerParts[] = {
            "PlayerBody", "PlayerHead", "PlayerArmL", "PlayerArmR"
        };
        for (const char *name : kPlayerParts) {
            auto *obj = GetObject3D(name);
            if (!obj) continue;
            auto *mat = obj->GetComponent3D<Material3D>();
            if (!mat) continue;
            auto c = mat->GetColor();
            c.w = 0.0f;
            mat->SetColor(c);
        }
    };

    float gameOverDanger = 0.0f;
    if (playState_ == PlayState::Playing && player_) {
        auto *playerTr = player_->GetComponent3D<Transform3D>();
        if (playerTr) {
            const Vector3 playerPos = playerTr->GetTranslate();

            if (noiseWallController_) {
                const float wallZ = noiseWallController_->GetWallPositionZ();
                const float dz = playerPos.z - wallZ;
                gameOverDanger = std::max(gameOverDanger, std::clamp(1.0f - ((-dz) / std::max(0.0001f, gameOverWallDangerDistance_)), 0.0f, 1.0f));
                if (dz >= 0.0f) {
                    playState_ = PlayState::GameOver;
                    if (playerMovementController_) {
                        playerMovementController_->SetMovementLocked(true);
                    }
                    hidePlayerVisuals();
                    if (gameOverUIController_ && !gameOverUIController_->IsActive()) {
                        gameOverUIController_->Activate();
                    }
                }
            }

            const float radial = std::sqrt(playerPos.x * playerPos.x + playerPos.y * playerPos.y);
            const float boundaryStart = stageBoundaryRadius_ * 0.7f;
            gameOverDanger = std::max(gameOverDanger, std::clamp((radial - boundaryStart) / std::max(0.0001f, stageBoundaryRadius_ - boundaryStart), 0.0f, 1.0f));
            if (radial >= stageBoundaryRadius_) {
                playState_ = PlayState::GameOver;
                if (playerMovementController_) {
                    playerMovementController_->SetMovementLocked(true);
                }
                hidePlayerVisuals();
                if (gameOverUIController_ && !gameOverUIController_->IsActive()) {
                    gameOverUIController_->Activate();
                }
            }

            if (goalPlaneController_ && playerPos.z < goalPlaneController_->GetGoalZ()) {
                playState_ = PlayState::Cleared;
                if (!clearSlowdownActive_) {
                    clearSlowdownActive_ = true;
                    clearSlowdownElapsed_ = 0.0f;
                    clearSlowdownStartGameSpeed_ = std::max(0.0f, GetGameSpeed());
                }
                if (noiseWallController_) {
                    noiseWallController_->SetMovementEnabled(false);
                }
                if (auto *camSync = GetSceneComponent<CameraToPlayerSync>()) {
                    camSync->SetClearViewEnabled(true);
                }
                const int touched = stageGroundGenerator_ ? stageGroundGenerator_->GetTouchedGroundCount() : 0;
                if (gameClearUIController_ && !gameClearUIController_->IsActive()) {
                    gameClearUIController_->Activate(touched);
                }
            }
        }
    }

    if (vignetteEffect_) {
        auto v = vignetteEffect_->GetParams();
        const Vector4 red{1.0f, 0.0f, 0.0f, 1.0f};
        v.color = Vector4::Lerp(baseVignetteColor_, red, gameOverDanger);
        v.intensity = std::max(v.intensity, gameOverDanger * 0.8f);
        if (playState_ == PlayState::GameOver) {
            v.intensity = std::max(v.intensity, 0.8f);
            v.color = red;
        }
        vignetteEffect_->SetParams(v);
    }

    if (playState_ == PlayState::Cleared && clearSlowdownActive_) {
        clearSlowdownElapsed_ += std::max(0.0f, GetDeltaTime());
        const float t = std::clamp(clearSlowdownElapsed_ / std::max(0.0001f, clearSlowdownDuration_), 0.0f, 1.0f);
        const float speed = (1.0f - t) * clearSlowdownStartGameSpeed_;
        SetGameSpeed(speed);
        if (t >= 1.0f) {
            clearSlowdownActive_ = false;
            SetGameSpeed(0.0f);
            if (playerMovementController_) {
                playerMovementController_->SetMovementLocked(true);
            }
            hidePlayerVisuals();
        }
    }

    const bool modalVisible = (gameOverUIController_ && gameOverUIController_->IsActive())
        || (gameClearUIController_ && gameClearUIController_->IsActive())
        || (pauseUIController_ && pauseUIController_->IsActive());
    if (gameSceneUIController_) {
        gameSceneUIController_->SetVisible(!modalVisible);
    }

    if (auto *ic = GetInputCommand()) {
        if (playState_ == PlayState::Playing && !modalVisible && ic->Evaluate("Cancel").Triggered()) {
            if (pauseUIController_) {
                pauseUIController_->Activate();
            }
        }
    }

    if (playState_ == PlayState::GameOver) {
        if (playerMovementController_) {
            playerMovementController_->SetMovementLocked(true);
        }
        hidePlayerVisuals();
    }

    if (playState_ == PlayState::Cleared && !clearSlowdownActive_) {
        if (playerMovementController_) {
            playerMovementController_->SetMovementLocked(true);
        }
        hidePlayerVisuals();
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

        const bool isGameOverNow = (playState_ == PlayState::GameOver);
        if (isGameOverNow && !wasGameOverPrevFrame_) {
            particleManager_->SetParentTransform("PlayerDeath", playerTr);
            particleManager_->Spawn("PlayerDeath", Vector3(0.0f, 0.0f, 0.0f));
        }
        wasGameOverPrevFrame_ = isGameOverNow;
    }

    if (gameOverUIController_) {
        const auto action = gameOverUIController_->ConsumeRequestedAction();
        if (action == GameOverUIController::RequestAction::Retry && GetNextSceneName().empty()) {
            SetNextSceneName("GameScene");
        } else if (action == GameOverUIController::RequestAction::BackToTitle && GetNextSceneName().empty()) {
            SetNextSceneName("TitleScene");
        }
    }

    if (gameClearUIController_) {
        const auto action = gameClearUIController_->ConsumeRequestedAction();
        if (action == GameClearUIController::RequestAction::Retry && GetNextSceneName().empty()) {
            SetNextSceneName("GameScene");
        } else if (action == GameClearUIController::RequestAction::BackToTitle && GetNextSceneName().empty()) {
            SetNextSceneName("TitleScene");
        }
    }

    if (pauseUIController_) {
        const auto action = pauseUIController_->ConsumeRequestedAction();
        if (action == PauseUIController::RequestAction::Continue) {
            pauseUIController_->Deactivate();
        } else if (action == PauseUIController::RequestAction::Retry && GetNextSceneName().empty()) {
            SetNextSceneName("GameScene");
        } else if (action == PauseUIController::RequestAction::BackToTitle && GetNextSceneName().empty()) {
            SetNextSceneName("TitleScene");
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
            if (GetNextSceneName().empty()) {
                SetNextSceneName("TitleScene");
            }
            if (auto *out = GetSceneComponent<SceneChangeOut>()) {
                out->Play();
            }
        }
#endif
    }
}

} // namespace KashipanEngine
