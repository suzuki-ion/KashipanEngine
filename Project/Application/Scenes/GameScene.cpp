#include "Scenes/GameScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Scenes/Components/CameraController.h"
#include "Objects/CreateFunc/CreateEnemyObject.h"
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
        op.threshold = 0.25f;
        op.thickness = 5.0f;
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

    AddSceneComponent(std::make_unique<CameraController>("Camera3D_ScreenBuffer3D"));
    if (auto *camCtrl = GetSceneComponent<CameraController>()) {
        camCtrl->SetFollowOffset(Vector3(0.0f, 0.0f, -16.0f));
    }

    AddObject3D(CreateEnemyObject(GetSceneContext()));
    AddObject3D(CreateGroundObject(GetSceneContext()));
    AddObject3D(CreatePlayerObject(GetSceneContext()));
    AddObject3D(CreateSkyboxObject(GetSceneContext()));

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());
    AddSceneComponent(std::make_unique<ParticleManager>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }

    if (auto *pm = GetSceneComponent<ParticleManager>()) {
        pm->LoadFromJsonFile("HitEffect.json");
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

    if (auto *playrer = GetObject3D("Player")) {
        if (auto *camCtrl = GetSceneComponent<CameraController>()) {
            camCtrl->SetTargetTranslate(playrer->GetComponent3D<Transform3D>()->GetTranslate() + Vector3(0.0f, 0.0f, -16.0f));
        }
    }

    if (auto *enemy = GetObject3D("Enemy")) {
        if (!enemy->GetComponent3D<EnemyAliveStateController>()->IsAlive()) {
            RemoveObject3D(enemy);
        }
    } else {
        static float timer = 0.0f;
        static const float spawnInterval = 1.0f;
        timer += GetDeltaTime() * GetGameSpeed();
        if (timer >= spawnInterval) {
            timer = 0.0f;
            AddObject3D(CreateEnemyObject(GetSceneContext()));
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

    // 確認用のコンポーネント一覧表示用ImGui
    ImGui::Begin("Component Registry");
    ImGui::Text("Scene Components:");
    for (const auto &comp : GetRegisteredSceneComponentTypes()) {
        ImGui::Text("\t%s", comp.c_str());
    }
    ImGui::Separator();
    ImGui::Text("Object2D Components:");
    for (const auto &comp : GetRegisteredObject2DComponentTypes()) {
        ImGui::Text("\t%s", comp.c_str());
    }
    ImGui::Separator();
    ImGui::Text("Object3D Components:");
    for (const auto &comp : GetRegisteredObject3DComponentTypes()) {
        ImGui::Text("\t%s", comp.c_str());
    }
    ImGui::End();

}

} // namespace KashipanEngine
