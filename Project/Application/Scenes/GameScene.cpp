#include "Scenes/GameScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

namespace KashipanEngine {

GameScene::GameScene()
    : SceneBase("GameScene") {}

void GameScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();

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
