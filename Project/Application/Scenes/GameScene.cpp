#include "Scenes/GameScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Scenes/Components/CameraController.h"
#include "Objects/CreateFunc/CreateGroundObject.h"
#include "Objects/CreateFunc/CreatePlayerObject.h"
#include "Objects/CreateFunc/CreateSkyboxObject.h"

namespace KashipanEngine {

GameScene::GameScene()
    : SceneBase("GameScene") {}

void GameScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();
    auto *mainCamera3D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetMainCamera3D() : nullptr;
    auto *screenBuffer3D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetScreenBuffer3D() : nullptr;

    if (screenBuffer3D) {
        auto op = OutlineEffect::Params{};
        op.threshold = 0.05f;
        op.thickness = 2.0f;
        op.color[0] = 0.0f;
        op.color[1] = 0.0f;
        op.color[2] = 0.0f;
        op.color[3] = 1.0f;
        op.cameraNear = mainCamera3D ? mainCamera3D->GetNearClip() : 0.1f;
        op.cameraFar = mainCamera3D ? mainCamera3D->GetFarClip() : 1000.0f;
        auto outlineEffect = std::make_unique<OutlineEffect>(op);
        screenBuffer3D->RegisterPostEffectComponent(std::move(outlineEffect));

        screenBuffer3D->AttachToRenderer("ScreenBuffer3D");
    }

    AddSceneComponent(std::make_unique<CameraController>(mainCamera3D));
    if (auto *camCtrl = GetSceneComponent<CameraController>()) {
        camCtrl->SetFollowOffset(Vector3(0.0f, 2.0f, -16.0f));
    }

    AddObject3D(CreateGroundObject(GetSceneContext()));
    AddObject3D(CreatePlayerObject(GetSceneContext()));
    AddObject3D(CreateSkyboxObject(GetSceneContext()));

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());
    AddSceneComponent(std::make_unique<ParticleManager>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }

    AddSceneComponent(std::make_unique<ParticleManager>());
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

    if (auto *playrer = GetObject3D("Player")) {
        if (auto *camCtrl = GetSceneComponent<CameraController>()) {
            camCtrl->SetTargetTranslate(playrer->GetComponent3D<Transform3D>()->GetTranslate() + Vector3(0.0f, 2.0f, -16.0f));
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
