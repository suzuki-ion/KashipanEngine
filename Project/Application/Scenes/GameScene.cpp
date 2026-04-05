#include "Scenes/GameScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Scenes/Components/CameraController.h"
#include "Scenes/Components/CameraToPlayerSync.h"
#include "Scenes/Components/StageObjectRandomGenerator.h"

#include "Objects/GameObjects/3D/Box.h"
#include "Objects/Components/PlayerMovement.h"
#include "Objects/Components/PlayerInputHandler.h"

namespace KashipanEngine {

GameScene::GameScene()
    : SceneBase("GameScene") {}

void GameScene::Initialize() {
    SetGameSpeed(1.0f);

    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();
    auto *screenBuffer3D = sceneDefaultVariables_->GetScreenBuffer3D();
    auto *directionalLight = sceneDefaultVariables_->GetDirectionalLight();
    directionalLight->SetDirection({ 0.0f, 0.0f, 1.0f });

    if (screenBuffer3D) {
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<ChromaticAberrationEffect>());

        BloomEffect::Params p;
        p.intensity = 0.25f;
        p.blurRadius = 4.0f;
        p.iterations = 4;
        p.softKnee = 2.5f;
        p.threshold = 0.2f;
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<BloomEffect>(p));
    }

    if (sceneDefaultVariables_) {
        if (auto *mainCamera = sceneDefaultVariables_->GetMainCamera3D()) {
            AddSceneComponent(std::make_unique<CameraController>(mainCamera));
        }

        AddSceneComponent(std::make_unique<StageObjectRandomGenerator>());

        if (auto *colliderComp = sceneDefaultVariables_->GetColliderComp()) {
            auto player = std::make_unique<Box>();
            player->SetName("Player");

            if (screenBuffer3D) {
                player->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            }

            if (auto *tr = player->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(Vector3{0.0f, 0.0f, -2.0f});
            }

            player->RegisterComponent<PlayerMovement>(colliderComp->GetCollider());
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

            AddSceneComponent(std::make_unique<CameraToPlayerSync>(playerPtr));
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
