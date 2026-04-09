#include "Scenes/GameScene.h"
#include "Scenes/GameScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Scenes/Components/CameraController.h"
#include "Scenes/Components/CameraToPlayerSync.h"
#include "Scenes/Components/PostEffectToPlayerSync.h"
#include "Scenes/Components/GameSceneUIController.h"
#include "Scenes/Components/StageGroundGenerator.h"
#include "Scenes/Components/StageNoiseWallController.h"
#include "Scenes/Components/StageGoalPlaneController.h"
#include "Scenes/Components/StageDecoPlaneGenerator.h"
#include "Scenes/Components/StageDecoBoxGenerator.h"
#include "Scenes/Components/StageObjectController.h"
#include "Scenes/Components/StageObjectRandomGenerator.h"

#include "Objects/GameObjects/3D/Box.h"
#include "Objects/Components/PlayerMovementController.h"
#include "Objects/Components/PlayerInputHandler.h"

#include <algorithm>

namespace KashipanEngine {

GameScene::GameScene()
    : SceneBase("GameScene") {}

void GameScene::Initialize() {
    SetGameSpeed(1.0f);

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

        stageGroundGenerator_ = GetSceneComponent<StageGroundGenerator>();
        noiseWallController_ = GetSceneComponent<StageNoiseWallController>();
        goalPlaneController_ = GetSceneComponent<StageGoalPlaneController>();
        gameSceneUIController_ = GetSceneComponent<GameSceneUIController>();

        if (auto *colliderComp = sceneDefaultVariables_->GetColliderComp()) {
            auto player = std::make_unique<Box>();
            player->SetName("Player");
            player->SetUniqueBatchKey();

            if (screenBuffer3D) {
                player->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            }

            if (auto *tr = player->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(Vector3{0.0f, 0.0f, -2.0f});
            }
            if (auto *mat = player->GetComponent3D<Material3D>()) {
                mat->SetTexture(TextureManager::GetTextureFromFileName("square_alpha.png"));
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

            AddSceneComponent(std::make_unique<CameraToPlayerSync>(playerPtr));
            AddSceneComponent(std::make_unique<PostEffectToPlayerSync>(playerPtr, radialBlurEffect, vignetteEffect));
        }

    }

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());
    AddSceneComponent(std::make_unique<ParticleManager>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }
}

GameScene::~GameScene() {}

void GameScene::OnUpdate() {
    if (!player_) {
        player_ = GetObject3D("Player");
    }
    if (player_ && !playerMovementController_) {
        playerMovementController_ = player_->GetComponent3D<PlayerMovementController>();
    }

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
                }
            }

            const float radial = std::sqrt(playerPos.x * playerPos.x + playerPos.y * playerPos.y);
            const float boundaryStart = stageBoundaryRadius_ * 0.75f;
            gameOverDanger = std::max(gameOverDanger, std::clamp((radial - boundaryStart) / std::max(0.0001f, stageBoundaryRadius_ - boundaryStart), 0.0f, 1.0f));
            if (radial >= stageBoundaryRadius_) {
                playState_ = PlayState::GameOver;
            }

            if (goalPlaneController_ && playerPos.z < goalPlaneController_->GetGoalZ()) {
                playState_ = PlayState::Cleared;
                if (playerMovementController_) {
                    playerMovementController_->SetMovementLocked(true);
                }
                if (gameSceneUIController_) {
                    const int touched = stageGroundGenerator_ ? stageGroundGenerator_->GetTouchedGroundCount() : 0;
                    gameSceneUIController_->StartClearPresentation(touched);
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

    if (playState_ == PlayState::GameOver) {
        if (GetNextSceneName().empty()) {
            SetNextSceneName("TitleScene");
            if (auto *out = GetSceneComponent<SceneChangeOut>()) {
                out->Play();
            }
        }
    }

    if (playState_ == PlayState::Cleared) {
        if (playerMovementController_) {
            playerMovementController_->SetMovementLocked(true);
        }
        if (gameSceneUIController_ && gameSceneUIController_->ConsumeRequestedReturnToTitle()) {
            if (GetNextSceneName().empty()) {
                SetNextSceneName("TitleScene");
            }
            if (auto *out = GetSceneComponent<SceneChangeOut>()) {
                out->Play();
            }
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
