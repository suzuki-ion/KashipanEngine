#include "Scenes/GameScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Scenes/Components/CameraController.h"
#include "Scenes/Components/CameraToPlayerSync.h"
#include "Scenes/Components/PostEffectToPlayerSync.h"
#include "Scenes/Components/GameSceneUIController.h"
#include "Scenes/Components/StageGroundGenerator.h"
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
        AddSceneComponent(std::make_unique<GameSceneUIController>());

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
                "PlayerGravitySwitchTrigger",
                "PlayerGravitySwitchRelease",
                "PlayerGravityUp",
                "PlayerGravityDown",
                "PlayerGravityLeft",
                "PlayerGravityRight");

            Object3DBase *playerPtr = player.get();
            AddObject3D(std::move(player));

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
