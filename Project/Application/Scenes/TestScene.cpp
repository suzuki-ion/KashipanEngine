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
    auto *screenBuffer2D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetScreenBuffer2D() : nullptr;

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());
    AddSceneComponent(std::make_unique<ParticleManager>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }

    AddSceneComponent(std::make_unique<ParticleManager>());
    AddSceneComponent(std::make_unique<KeyframeAnimator>());
    if (auto *pm = GetSceneComponent<ParticleManager>()) {
        pm->LoadFromJsonFile("HitEffect.json");
    }
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

        auto gfp = GaussianFilterEffect::Params{};
        gfp.radius = 3;
        gfp.sigma = 1.0f;
        auto gaussianFilterEffect = std::make_unique<GaussianFilterEffect>(gfp);
        screenBuffer3D->RegisterPostEffectComponent(std::move(gaussianFilterEffect));

        auto cap = ColorAdjustEffect::Params{};
        auto colorAdjustEffect = std::make_unique<ColorAdjustEffect>(cap);
        screenBuffer3D->RegisterPostEffectComponent(std::move(colorAdjustEffect));

        screenBuffer3D->AttachToRenderer("ScreenBuffer3D");
    }

    if (screenBuffer3D) {
        auto box = std::make_unique<Box>();
        box->SetName("TestBox");

        if (auto *material = box->GetComponent3D<Material3D>()) {
            auto texture = TextureManager::GetTextureFromFileName("uvChecker.png");
            auto envTexture = TextureManager::GetTextureFromFileName("rostock_laage_airport_4k.dds");
            material->SetTexture(texture);
            material->SetEnvironmentTexture(envTexture);
        }
        if (auto *transform = box->GetComponent3D<Transform3D>()) {
            transform->SetTranslate(Vector3(0.0f, 0.0f, 0.0f));
            transform->SetScale(Vector3(1.0f, 1.0f, 1.0f));
        }

        box->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(box));
    }

    if (screenBuffer3D) {
        auto handle = ModelManager::GetModelHandleFromFileName("AnimatedCube.gltf");
        auto animatedCube = std::make_unique<Model>(handle);
        animatedCube->SetName("AnimatedCube");
        if (auto *transform = animatedCube->GetComponent3D<Transform3D>()) {
            transform->SetTranslate(Vector3(-2.0f, 0.0f, 0.0f));
            transform->SetScale(Vector3(1.0f, 1.0f, 1.0f));
        }
        animatedCube->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(animatedCube));

        auto *keyframeAnimator = GetSceneComponent<KeyframeAnimator>();
        if (keyframeAnimator) {
            auto animationHandle = AnimationManager::GetAnimationHandleFromFileName("AnimatedCube.gltf");
            keyframeAnimator->PlayFromAnimationHandle(animationHandle, "AnimatedCube", true);
        }
    }

    if (auto *keyframeAnimator = GetSceneComponent<KeyframeAnimator>()) {
        auto skeletonHandle = SkeletonManager::GetSkeletonHandleFromFileName("walk.gltf");
        auto animationHandle = AnimationManager::GetAnimationHandleFromFileName("walk.gltf");
        keyframeAnimator->PlayFromAnimationAndSkeletonHandle(animationHandle, skeletonHandle, true);
    }

    if (screenBuffer3D) {
        auto ring = std::make_unique<Ring3D>();
        ring->SetUvMode(Ring3D::UvMode::Curved);
        ring->SetName("TestRing");
        if (auto *material = ring->GetComponent3D<Material3D>()) {
            auto texture = TextureManager::GetTextureFromFileName("gradationLine.png");
            material->SetTexture(texture);
        }
        if (auto *transform = ring->GetComponent3D<Transform3D>()) {
            transform->SetTranslate(Vector3(2.0f, 0.0f, 0.0f));
            transform->SetScale(Vector3(1.0f, 1.0f, 1.0f));
        }
        ring->RegisterComponent(std::make_unique<LookAtConstraint>());
        if (auto *constraint = ring->GetComponent3D<LookAtConstraint>()) {
            constraint->SetTargetFunc([this, mainCamera3D]() -> const Vector3 & {
                if (mainCamera3D) {
                    if (auto *camTr = mainCamera3D->GetComponent3D<Transform3D>()) {
                        return camTr->GetTranslate();
                    }
                }
                static const Vector3 defaultTarget(0.0f, 0.0f, 0.0f);
                return defaultTarget;
                });
        }
        ring->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(ring));
    }

    if (screenBuffer3D) {
        auto cylinder = std::make_unique<Cylinder3D>();
        cylinder->SetName("TestCylinder");
        if (auto *material = cylinder->GetComponent3D<Material3D>()) {
            auto texture = TextureManager::GetTextureFromFileName("uvChecker.png");
            material->SetTexture(texture);
        }
        if (auto *transform = cylinder->GetComponent3D<Transform3D>()) {
            transform->SetTranslate(Vector3(0.0f, 2.0f, 0.0f));
            transform->SetScale(Vector3(1.0f, 1.0f, 1.0f));
        }
        cylinder->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(cylinder));
    }

    if (screenBuffer2D) {
        auto ring = std::make_unique<Ring2D>();
        ring->SetUvMode(Ring2D::UvMode::Curved);
        ring->SetName("TestRing2D");
        if (auto *material = ring->GetComponent2D<Material2D>()) {
            auto texture = TextureManager::GetTextureFromFileName("gradationLine.png");
            material->SetTexture(texture);
        }
        if (auto *transform = ring->GetComponent2D<Transform2D>()) {
            transform->SetTranslate(Vector2(200.0f, 200.0f));
            transform->SetScale(Vector2(100.0f, 100.0f));
        }
        ring->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        AddObject2D(std::move(ring));
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
