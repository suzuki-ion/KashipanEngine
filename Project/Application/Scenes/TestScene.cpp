#include "Scenes/TestScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

#include <array>

namespace KashipanEngine {

TestScene::TestScene()
    : SceneBase("TestScene") {}

void TestScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();
    auto *screenBuffer3D = sceneDefaultVariables_->GetScreenBuffer3D();
    auto *mainCamera3D = sceneDefaultVariables_->GetMainCamera3D();

    Transform3D *playerRootTr = nullptr;
    
    {
        auto obj = std::make_unique<Plane3D>();
        obj->SetName("Ground");
        obj->SetUniqueBatchKey();
        if (screenBuffer3D) {
            obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        }
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3{0.0f, 0.0f, 0.0f});
            tr->SetScale(Vector3{32.0f, 32.0f, 32.0f});
            tr->SetRotate(Vector3{ 1.57079632679f, 0.0f, 0.0f });
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetEnableLighting(true);
            mat->SetSampler(SamplerManager::GetSampler(DefaultSampler::LinearWrap));
            mat->SetTexture(TextureManager::GetTextureFromFileName("white1x1.png"));
            mat->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
        }
        AddObject3D(std::move(obj));
    }
    {
        auto obj = std::make_unique<Box>();
        obj->SetName("PlayerRoot");
        obj->SetUniqueBatchKey();
        if (screenBuffer3D) {
            obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        }
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3{0.0f, 0.0f, 0.0f});
            tr->SetScale(Vector3{1.0f, 1.0f, 1.0f});
            playerRootTr = tr;
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetTexture(TextureManager::GetTextureFromFileName("square_alpha.png"));
        }
        AddObject3D(std::move(obj));
    }
    {
        auto modelHandle = ModelManager::GetModelHandleFromFileName("float_Body.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetName("PlayerBody");
        if (screenBuffer3D) {
            obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        }
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetParentTransform(playerRootTr);
            tr->SetTranslate(Vector3{0.0f, 0.0f, 0.0f});
            tr->SetScale(Vector3{1.0f, 1.0f, 1.0f});
        }
        AddObject3D(std::move(obj));
    }
    {
        auto modelHandle = ModelManager::GetModelHandleFromFileName("float_Head.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetName("PlayerHead");
        if (screenBuffer3D) {
            obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        }
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetParentTransform(playerRootTr);
            tr->SetTranslate(Vector3{0.0f, 0.0f, 0.0f});
            tr->SetScale(Vector3{1.0f, 1.0f, 1.0f});
        }
        AddObject3D(std::move(obj));
    }
    {
        auto modelHandle = ModelManager::GetModelHandleFromFileName("float_L_arm.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetName("PlayerArmL");
        if (screenBuffer3D) {
            obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        }
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetParentTransform(playerRootTr);
            tr->SetTranslate(Vector3{0.0f, 0.0f, 0.0f});
            tr->SetScale(Vector3{1.0f, 1.0f, 1.0f});
        }
        AddObject3D(std::move(obj));
    }
    {
        auto modelHandle = ModelManager::GetModelHandleFromFileName("float_R_arm.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetName("PlayerArmR");
        if (screenBuffer3D) {
            obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        }
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetParentTransform(playerRootTr);
            tr->SetTranslate(Vector3{0.0f, 0.0f, 0.0f});
            tr->SetScale(Vector3{1.0f, 1.0f, 1.0f});
        }
        AddObject3D(std::move(obj));
    }

    AddSceneComponent(std::make_unique<ParticleManager>());
    AddSceneComponent(std::make_unique<ModelAnimator>());
    if (mainCamera3D) {
        auto debugCameraMovement = std::make_unique<DebugCameraMovement>(mainCamera3D);
        debugCameraMovement->SetEnable(true);
        AddSceneComponent(std::move(debugCameraMovement));
    }

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }
}

TestScene::~TestScene() {}

void TestScene::OnUpdate() {
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
