#include "Scenes/TestScene.h"
#include "Objects/Components/ParticleMovement.h"

namespace KashipanEngine {

TestScene::TestScene()
    : SceneBase("TestScene") {
}

void TestScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();
    auto *mainCamera3D = sceneDefaultVariables_->GetMainCamera3D();
    auto *screenBuffer3D = sceneDefaultVariables_->GetScreenBuffer3D();
    auto *shadowMapBuffer = sceneDefaultVariables_->GetShadowMapBuffer();
    auto *shadowMapCameraSync = sceneDefaultVariables_->GetShadowMapCameraSync();
    auto *directionalLight = sceneDefaultVariables_->GetDirectionalLight();
    auto *lightManager = sceneDefaultVariables_->GetLightManager();
    auto whiteTexture = TextureManager::GetTextureFromFileName("white1x1.png");

    // デバッグ用カメラ操作コンポーネント
    {
        auto cmp = std::make_unique<DebugCameraMovement>(mainCamera3D, GetInput());
        cmp->SetCenter({ 0.0f, 5.0f, 0.0f });
        cmp->SetDistance(50.0f);
        cmp->SetAngles(-M_PI / 2.0f, M_PI / 2.0f);
        AddSceneComponent(std::move(cmp));
    }
    
    // 地面用Box
    {
        auto obj = std::make_unique<Box>();
        obj->SetName("GroundBox");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetScale({ 32.0f, 1.0f, 32.0f });
            tr->SetTranslate({ 0.0f, -1.5f, 0.0f });
        }
        if (auto *mt = obj->GetComponent3D<Material3D>()) {
            mt->SetTexture(whiteTexture);
        }
        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        AddObject3D(std::move(obj));
    }

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

    // ライト調整
    if (directionalLight) {
        directionalLight->SetColor({ 0.5f, 0.5f, 1.0f, 1.0f });
        directionalLight->SetDirection({ 1.0f, -0.5f, 1.0f });
        directionalLight->SetIntensity(0.2f);
    }

    // シャドウマッピングカメラ同期設定
    if (shadowMapCameraSync) {
        shadowMapCameraSync->SetShadowNear(1.0f);
        shadowMapCameraSync->SetShadowFar(128.0f);
    }

    // スポットライト
    {
        auto sl1 = std::make_unique<SpotLight>();
        sl1->SetName("SpotLight1");
        sl1->SetColor({ 1.0f, 0.5f, 0.5f, 1.0f });
        sl1->SetPosition({ 10.0f, 20.0f, -10.0f });
        sl1->SetDirection({ -0.5f, -1.0f, 0.5f });
        sl1->SetRange(32.0f);
        sl1->SetInnerAngle(0.3f);
        sl1->SetOuterAngle(0.35f);
        sl1->SetIntensity(2.0f);

        auto sl2 = std::make_unique<SpotLight>();
        sl2->SetName("SpotLight2");
        sl2->SetColor({ 0.5f, 0.5f, 1.0f, 1.0f });
        sl2->SetPosition({ -15.0f, 25.0f, 15.0f });
        sl2->SetDirection({ 0.5f, -1.0f, -0.5f });
        sl2->SetRange(32.0f);
        sl2->SetInnerAngle(0.3f);
        sl2->SetOuterAngle(0.35f);
        sl2->SetIntensity(2.0f);

        auto sl3 = std::make_unique<SpotLight>();
        sl3->SetName("SpotLight3");
        sl3->SetColor({ 0.5f, 1.0f, 0.5f, 1.0f });
        sl3->SetPosition({ 0.0f, 30.0f, 0.0f });
        sl3->SetDirection({ 0.0f, -1.0f, 0.0f });
        sl3->SetRange(32.0f);
        sl3->SetInnerAngle(0.3f);
        sl3->SetOuterAngle(0.35f);
        sl3->SetIntensity(2.0f);

        if (screenBuffer3D) {
            sl1->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            sl2->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            sl3->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        }
        if (lightManager) {
            lightManager->AddSpotLight(sl1.get());
            lightManager->AddSpotLight(sl2.get());
            lightManager->AddSpotLight(sl3.get());
        }
        AddObject3D(std::move(sl1));
        AddObject3D(std::move(sl2));
        AddObject3D(std::move(sl3));
    }

    // パーティクル
    {
        ParticleMovement::SpawnBox spawn;
        spawn.min = Vector3(-16.0f, 0.0f, -16.0f);
        spawn.max = Vector3(16.0f, 16.0f, 16.0f);

        int instanceCount = 32;

        particleLights_.clear();
        particleLights_.reserve(instanceCount);

        for (int i = 0; i < instanceCount; ++i) {
            auto obj = std::make_unique<Box>();
            obj->SetName("ParticleBox_" + std::to_string(i));
            obj->RegisterComponent<ParticleMovement>(spawn, 0.5f, 10.0f, Vector3{ 0.5f, 0.5f, 0.5f });

            if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                tr->SetScale(Vector3(0.5f, 0.5f, 0.5f));
            }
            if (auto *mat = obj->GetComponent3D<Material3D>()) {
                mat->SetEnableLighting(true);
                mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            }

            if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");

            auto *particlePtr = obj.get();
            AddObject3D(std::move(obj));

            auto lightObj = std::make_unique<PointLight>();
            lightObj->SetName("ParticlePointLight_" + std::to_string(i));
            lightObj->SetEnabled(true);
            lightObj->SetColor(particleLightColor_);
            lightObj->SetIntensity(0.0f);
            lightObj->SetRange(0.0f);

            auto *lightPtr = lightObj.get();
            if (screenBuffer3D) lightObj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            AddObject3D(std::move(lightObj));

            particleLights_.push_back(ParticleLightPair{ particlePtr, lightPtr });
            lightManager->AddPointLight(lightPtr);
        }
    }

    if (screenBuffer3D) {
        auto chromaticAberrationParams = ChromaticAberrationEffect::Params{};
        chromaticAberrationParams.directionX = 1.0f;
        chromaticAberrationParams.directionY = 0.0f;
        chromaticAberrationParams.strength = 0.002f;
        auto chromaticAberration = std::make_unique<ChromaticAberrationEffect>(chromaticAberrationParams);
        screenBuffer3D->RegisterPostEffectComponent(std::move(chromaticAberration));

        auto bloomParams = BloomEffect::Params{};
        bloomParams.threshold = 1.0f;
        bloomParams.softKnee = 0.5f;
        bloomParams.intensity = 1.0f;
        bloomParams.blurRadius = 2.0f;
        bloomParams.iterations = 4;
        auto bloom = std::make_unique<BloomEffect>(bloomParams);
        screenBuffer3D->RegisterPostEffectComponent(std::move(bloom));

        screenBuffer3D->AttachToRenderer("MainCamera3D");
    }
}

TestScene::~TestScene() {
}

void TestScene::OnUpdate() {
    {
        for (auto &pl : particleLights_) {
            if (!pl.particle || !pl.light) continue;

            auto *tr = pl.particle->GetComponent3D<Transform3D>();
            auto *pm = pl.particle->GetComponent3D<ParticleMovement>();
            if (!tr || !pm) continue;

            const Vector3 pos = tr->GetTranslate();
            pl.light->SetPosition(pos);

            const float t = pm->GetNormalizedLife(); // 0..1
            float life01 = 0.0f;
            if (t < 0.5f) {
                life01 = (t / 0.5f);
            } else {
                life01 = (1.0f - (t - 0.5f) / 0.5f);
            }
            life01 = std::clamp(life01, 0.0f, 1.0f);

            const float eased = EaseOutCubic(0.0f, 1.0f, life01);
            pl.light->SetIntensity(particleLightIntensityMin_ + (particleLightIntensityMax_ - particleLightIntensityMin_) * eased);
            pl.light->SetRange(particleLightRangeMin_ + (particleLightRangeMax_ - particleLightRangeMin_) * eased);
        }
    }
}

} // namespace KashipanEngine
