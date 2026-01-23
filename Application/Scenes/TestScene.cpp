#include "Scenes/TestScene.h"

namespace KashipanEngine {

TestScene::TestScene()
    : SceneBase("TestScene") {
}

void TestScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();
    auto *mainCamera3D = sceneDefaultVariables_->GetMainCamera3D();
    //auto *screenBuffer3D = sceneDefaultVariables_->GetScreenBuffer3D();
    //auto *shadowMapBuffer = sceneDefaultVariables_->GetShadowMapBuffer();
    auto *directionalLight = sceneDefaultVariables_->GetDirectionalLight();

    // カメラ位置調整
    if (mainCamera3D) {
        if (auto *tr = mainCamera3D->GetComponent3D<Transform3D>()) {
            tr->SetTranslate({ 0.0f, 5.0f, -15.0f });
            tr->SetRotate({ M_PI / 12.0f, 0.0f, 0.0f });// 15度
        }
    }

    // ライト方向調整
    if (directionalLight) {
        directionalLight->SetColor({ 0.5f, 0.5f, 1.0f, 1.0f });
        directionalLight->SetDirection({ -0.5f, -1.0f, -0.5f });
        directionalLight->SetIntensity(0.5f);
    }
}

TestScene::~TestScene() {
}

void TestScene::OnUpdate() {
    
}

} // namespace KashipanEngine
