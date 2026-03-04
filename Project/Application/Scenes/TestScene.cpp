#include "Scenes/TestScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

namespace KashipanEngine {

TestScene::TestScene()
    : SceneBase("TestScene") {}

void TestScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }
}

TestScene::~TestScene() {}

void TestScene::OnUpdate() {
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

    if (!GetNextSceneName().empty()) {
        if (auto *out = GetSceneComponent<SceneChangeOut>()) {
            if (out->IsFinished()) {
                ChangeToNextScene();
            }
        }
    }
}

} // namespace KashipanEngine
