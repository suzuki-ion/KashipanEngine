#include "Scenes/TestScene.h"
#include "Scenes/Components/PlayerHealthUI.h"
#include "Objects/Components/ParticleMovement.h"
#include "Objects/SystemObjects/ShadowMapBinder.h"
#include "Scene/Components/ShadowMapCameraSync.h"
#include "objects/Components/Health.h"

namespace KashipanEngine {

TestScene::TestScene()
    : SceneBase("TestScene") {
}

void TestScene::Initialize() {
    auto *defaultVariables = GetSceneComponent<SceneDefaultVariables>();
    auto *screenBuffer3D = defaultVariables ? defaultVariables->GetScreenBuffer3D() : nullptr;
    auto *shadowMapBuffer = defaultVariables ? defaultVariables->GetShadowMapBuffer() : nullptr;
    auto *lightManager = defaultVariables ? defaultVariables->GetLightManager() : nullptr;

    if (screenBuffer3D) {
        ChromaticAberrationEffect::Params p{};
        p.directionX = 1.0f;
        p.directionY = 0.0f;
        p.strength = 0.001f;
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<ChromaticAberrationEffect>(p));

        BloomEffect::Params bp{};
        bp.threshold = 1.0f;
        bp.softKnee = 0.25f;
        bp.intensity = 0.5f;
        bp.blurRadius = 1.0f;
        bp.iterations = 4;
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<BloomEffect>(bp));
        
        screenBuffer3D->AttachToRenderer("ScreenBuffer_TitleScene");
    }

    //==================================================
    // ↓ Here begins the game object definition ↓
    //==================================================

    // Floor Box
    {
        auto obj = std::make_unique<Box>();
        obj->SetUniqueBatchKey();
        obj->SetName("FloorBox");
        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, -0.5f, 0.0f));
            tr->SetScale(Vector3(64.0f, 1.0f, 64.0f));
        }
        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        AddObject3D(std::move(obj));
    }

    // Particles
    {
        ParticleMovement::SpawnBox spawn;
        spawn.min = Vector3(-16.0f, 0.0f, -16.0f);
        spawn.max = Vector3(16.0f, 16.0f, 16.0f);

        int instanceCount = 128;

        particleLights_.clear();
        particleLights_.reserve(instanceCount);

        for (int i = 0; i < instanceCount; ++i) {
            auto obj = std::make_unique<Box>();
            obj->SetName("ParticleBox_" + std::to_string(i));
            obj->RegisterComponent<ParticleMovement>(spawn, 0.5f, 10.0f, Vector3{0.5f, 0.5f, 0.5f});

            if (auto* mat = obj->GetComponent3D<Material3D>()) {
                mat->SetEnableLighting(true);
                mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            }

            if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");

            auto* particlePtr = obj.get();
            AddObject3D(std::move(obj));

            auto lightObj = std::make_unique<PointLight>();
            lightObj->SetName("ParticlePointLight_" + std::to_string(i));
            lightObj->SetEnabled(true);
            lightObj->SetColor(particleLightColor_);
            lightObj->SetIntensity(0.0f);
            lightObj->SetRange(0.0f);

            auto* lightPtr = lightObj.get();
            if (screenBuffer3D) lightObj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            AddObject3D(std::move(lightObj));

            particleLights_.push_back(ParticleLightPair{ particlePtr, lightPtr });
            lightManager->AddPointLight(lightPtr);
        }
    }

    //==================================================
    // ↑ End of game object definition ↑
    //==================================================
}

TestScene::~TestScene() {
}

void TestScene::OnUpdate() {
    // Particle -> PointLight sync (position + lifetime-driven params)
    {
        for (auto &pl : particleLights_) {
            if (!pl.particle || !pl.light) continue;

            auto* tr = pl.particle->GetComponent3D<Transform3D>();
            auto* pm = pl.particle->GetComponent3D<ParticleMovement>();
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

    {
        auto r = GetInputCommand()->Evaluate("DebugDestroyWindow");
        if (r.Triggered()) {
            if (auto *window = Window::GetWindow("Main Window")) {
                window->DestroyNotify();
            }
        }
    }
}

} // namespace KashipanEngine