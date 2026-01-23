#include "Scenes/TestScene.h"

namespace KashipanEngine {

TestScene::TestScene()
    : SceneBase("TestScene") {
}

void TestScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();
    auto *mainCamera3D = sceneDefaultVariables_->GetMainCamera3D();
    auto *screenBuffer3D = sceneDefaultVariables_->GetScreenBuffer3D();
    auto *shadowMapBuffer = sceneDefaultVariables_->GetShadowMapBuffer();
    auto *directionalLight = sceneDefaultVariables_->GetDirectionalLight();
    auto whiteTexture = TextureManager::GetTextureFromFileName("white1x1.png");

    // シーン用オブジェクト
    {
        auto modelHandle = ModelManager::GetModelHandleFromFileName("scene.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetName("SceneModel");
        if (auto *mt = obj->GetComponent3D<Material3D>()) {
            mt->SetTexture(whiteTexture);
            mt->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            mt->SetShininess(2.0f);
        }
        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        AddObject3D(std::move(obj));
    }

    // カメラ位置調整
    if (mainCamera3D) {
        if (auto *tr = mainCamera3D->GetComponent3D<Transform3D>()) {
            tr->SetTranslate({ 0.0f, 22.0f, -50.0f });
            tr->SetRotate({ 0.3f, 0.0f, 0.0f });
        }
    }

    // ライト方向調整
    if (directionalLight) {
        directionalLight->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        directionalLight->SetDirection({ 1.0f, -0.5f, 1.0f });
        directionalLight->SetIntensity(1.0f);
    }
}

TestScene::~TestScene() {
}

void TestScene::OnUpdate() {
    
}

} // namespace KashipanEngine
