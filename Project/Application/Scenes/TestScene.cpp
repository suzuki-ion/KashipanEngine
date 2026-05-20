#include "Scenes/TestScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

namespace KashipanEngine {

TestScene::TestScene()
    : SceneBase("TestScene") {}

void TestScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();
    auto *mainCamera3D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetMainCamera3D() : nullptr;
    auto *screenBuffer3D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetScreenBuffer3D() : nullptr;

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());
    AddSceneComponent(std::make_unique<ParticleManager>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }

    AddSceneComponent(std::make_unique<ParticleManager>());
    if (mainCamera3D) {
        auto debugCameraMovement = std::make_unique<DebugCameraMovement>(mainCamera3D);
        debugCameraMovement->SetEnable(true);
        AddSceneComponent(std::move(debugCameraMovement));
    }

    if (screenBuffer3D) {
        auto gp = GrayscaleEffect::Params{};
        gp.intensity = 0.5f;
        auto postEffectComponent = std::make_unique<GrayscaleEffect>(gp);
        screenBuffer3D->RegisterPostEffectComponent(std::move(postEffectComponent));

        auto vp = VignetteEffect::Params{};
        vp.intensity = 1.0f;
        vp.innerRadius = 0.2f;
        vp.smoothness = 0.5f;
        auto vignetteEffect = std::make_unique<VignetteEffect>(vp);
        screenBuffer3D->RegisterPostEffectComponent(std::move(vignetteEffect));

        auto bfp = BoxFilter5x5Effect::Params{};
        bfp.intensity = 0.5f;
        auto boxFilterEffect = std::make_unique<BoxFilter5x5Effect>(bfp);
        screenBuffer3D->RegisterPostEffectComponent(std::move(boxFilterEffect));

        screenBuffer3D->AttachToRenderer("ScreenBuffer3D");
    }

    if (screenBuffer3D) {
        auto sphere = std::make_unique<Sphere>(512, 1024);
        sphere->SetName("TestSphere");

        if (auto *material = sphere->GetComponent3D<Material3D>()) {
            auto texture = TextureManager::GetTextureFromFileName("uvChecker.png");
            auto envTexture = TextureManager::GetTextureFromFileName("rostock_laage_airport_4k.dds");
            material->SetTexture(texture);
            material->SetEnvironmentTexture(envTexture);
        }

        if (auto *transform = sphere->GetComponent3D<Transform3D>()) {
            transform->SetTranslate(Vector3(0.0f, 0.0f, 0.0f));
            transform->SetScale(Vector3(1.0f, 1.0f, 1.0f));
        }

        sphere->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(sphere));
    }

    if (screenBuffer3D) {
        auto skybox = std::make_unique<Skybox>();
        skybox->SetName("TestSkybox");

        if (auto *material = skybox->GetComponent3D<Material3D>()) {
            auto cubemapTexture = TextureManager::GetTextureFromFileName("rostock_laage_airport_4k.dds");
            material->SetTexture(cubemapTexture);
        }

        if (auto *transform = skybox->GetComponent3D<Transform3D>()) {
            transform->SetScale(Vector3(100.0f, 100.0f, 100.0f));
        }

        skybox->AttachToRenderer(screenBuffer3D, "Skybox.Solid.BlendNormal");
        AddObject3D(std::move(skybox));
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
