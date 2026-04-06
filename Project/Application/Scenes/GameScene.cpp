#include "Scenes/GameScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Scenes/Components/CameraController.h"
#include "Scenes/Components/CameraToPlayerSync.h"
#include "Scenes/Components/PostEffectToPlayerSync.h"
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

    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();
    auto *screenBuffer3D = sceneDefaultVariables_->GetScreenBuffer3D();
    auto *screenBuffer2D = sceneDefaultVariables_->GetScreenBuffer2D();
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

        screenBuffer3D->AttachToRenderer("ScreenBuffer3D.Default");
    }

    if (sceneDefaultVariables_) {
        if (auto *mainCamera = sceneDefaultVariables_->GetMainCamera3D()) {
            AddSceneComponent(std::make_unique<CameraController>(mainCamera));
        }

        AddSceneComponent(std::make_unique<StageObjectRandomGenerator>());

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

            player->RegisterComponent<PlayerMovementController>(colliderComp->GetCollider());
            player->RegisterComponent<PlayerInputHandler>(
                GetInputCommand(),
                "PlayerMoveRight",
                "PlayerMoveLeft",
                "PlayerJump",
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
            AddSceneComponent(std::make_unique<PostEffectToPlayerSync>(playerPtr, radialBlurEffect));
        }

        if (screenBuffer2D) {
            auto speedBar = std::make_unique<SpriteProressBar>();
            speedBar->SetName("ForwardSpeedBar");
            speedBar->SetBarSize(Vector2{512.0f, 256.0f});
            speedBar->SetFrameThickness(4.0f);
            speedBar->SetFrameColor(Vector4{1.0f, 1.0f, 1.0f, 1.0f});
            speedBar->SetBackgroundColor(Vector4{0.1f, 0.1f, 0.1f, 1.0f});
            speedBar->SetBarColor(Vector4{0.0f, 0.5f, 0.0f, 1.0f});
            speedBar->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");

            if (auto *tr = speedBar->GetComponent2D<Transform2D>()) {
                tr->SetTranslate(Vector3{240.0f, 80.0f, 0.0f});
            }

            forwardSpeedBar_ = speedBar.get();
            AddObject2D(std::move(speedBar));
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
    if (!player_ || !playerMovementController_) {
        player_ = GetObject3D("Player");
        if (player_) {
            playerMovementController_ = player_->GetComponent3D<PlayerMovementController>();
        }
    }

    if (forwardSpeedBar_ && playerMovementController_) {
        const float speed = playerMovementController_->GetForwardSpeed();
        const float minSpeed = playerMovementController_->GetMinForwardSpeed();
        const float maxSpeed = playerMovementController_->GetMaxForwardSpeed();
        const float range = std::max(0.0001f, maxSpeed - minSpeed);
        const float progress = std::clamp((speed - minSpeed) / range, 0.0f, 1.0f);
        forwardSpeedBar_->SetProgress(progress);
    }

    if (!GetNextSceneName().empty()) {
        if (auto *out = GetSceneComponent<SceneChangeOut>()) {
            if (out->IsFinished()) {
                ChangeToNextScene();
            }
        }
    }

    if (auto *ic = GetInputCommand()) {
        if (ic->Evaluate("DebugSceneChange").Triggered()) {
            if (GetNextSceneName().empty()) {
                SetNextSceneName("MenuScene");
            }
            if (auto *out = GetSceneComponent<SceneChangeOut>()) {
                out->Play();
            }
        }
    }
}

} // namespace KashipanEngine
