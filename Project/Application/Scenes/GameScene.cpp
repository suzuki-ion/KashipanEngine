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
    : Scene("GameScene") {}

void GameScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();
    auto *mainCamera3D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetMainCamera3D() : nullptr;
    auto *screenBuffer3D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetScreenBuffer3D() : nullptr;

    if (screenBuffer3D) {
        auto op = OutlineEffect::Params{};
        op.threshold = 0.25f;
        op.thickness = 5.0f;
        op.color[0] = 1.0f;
        op.color[1] = 1.0f;
        op.color[2] = 1.0f;
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

    AddObject(CreateEnemyObject(GetSceneContext(), Vector3(0.0f, 6.0f, 0.0f)));
    AddObject(CreateEnemyObject(GetSceneContext(), Vector3(2.0f, 6.0f, 0.0f)));
    AddObject(CreateEnemyObject(GetSceneContext(), Vector3(4.0f, 6.0f, 0.0f)));
    AddObject(CreateGroundObject(GetSceneContext()));
    AddObject(CreatePlayerObject(GetSceneContext()));

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

    if (auto *playrer = GetObject("Player")) {
        if (auto *camCtrl = GetSceneComponent<CameraController>()) {
            camCtrl->SetTargetTranslate(playrer->GetComponent3D<Transform3D>()->GetTranslate() + Vector3(0.0f, 0.0f, -16.0f));
        }
    }

    struct EnemySpawnInfo {
        Vector3 spawnPosition;
        float spawnInterval = 1.0f;
        float timer;
    };
    static std::vector<EnemySpawnInfo> enemySpawnInfos;
    for (auto *enemy : GetObjects3D("Enemy")) {
        auto *enemyAliveStateController = enemy->GetComponent3D<EnemyAliveStateController>();
        if (!enemyAliveStateController) continue;
        if (!enemyAliveStateController->IsAlive()) {
            EnemySpawnInfo spawnInfo;
            spawnInfo.spawnPosition = enemy->GetComponent3D<Transform3D>()->GetTranslate();
            spawnInfo.timer = 0.0f;
            enemySpawnInfos.push_back(spawnInfo);
            RemoveObject(enemy);
        }
    }
    for (auto it = enemySpawnInfos.begin(); it != enemySpawnInfos.end();) {
        it->timer += GetDeltaTime();
        if (it->timer >= it->spawnInterval) {
            AddObject(CreateEnemyObject(GetSceneContext(), it->spawnPosition));
            it = enemySpawnInfos.erase(it);
        } else {
            ++it;
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

#ifdef USE_IMGUI
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
    ImGui::Text("Object Components:");
    for (const auto &comp : GetRegisteredObjectComponentTypes()) {
        ImGui::Text("\t%s", comp.c_str());
    }
    ImGui::End();
#endif

}

} // namespace KashipanEngine
