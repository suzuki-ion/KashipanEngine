#include "Scenes/TestScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

namespace KashipanEngine {

TestScene::TestScene()
    : SceneBase("TestScene") {
}

void TestScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();
    auto *screenBuffer2D = sceneDefaultVariables_->GetScreenBuffer2D();

    {
        auto obj = std::make_unique<Sprite>();
        obj->SetName("TestSprite");
        if (auto *tr = obj->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector2(400.0f, 300.0f));
            tr->SetScale(Vector2(200.0f, 200.0f));
        }
        if (auto *mat = obj->GetComponent2D<Material2D>()) {
            mat->SetColor(Vector4(1.0f, 0.5f, 0.5f, 1.0f));
        }
        obj->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        AddObject2D(std::move(obj));
    }

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }
}

TestScene::~TestScene() {
}

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