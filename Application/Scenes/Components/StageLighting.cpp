#include "StageLighting.h"

namespace KashipanEngine {

StageLighting::StageLighting()
    : ISceneComponent("StageLighting", 1), lightManager_(nullptr) {}

void StageLighting::Initialize() {
    auto *ctx = GetOwnerContext();
    if (!ctx) return;
    auto *sceneDefaults = ctx->GetComponent<SceneDefaultVariables>();
    lightManager_ = sceneDefaults ? sceneDefaults->GetLightManager() : nullptr;
    auto *screenBuffer3D = sceneDefaults ? sceneDefaults->GetScreenBuffer3D() : nullptr;

    // 適当にスポットライトとポイントライトを追加しておく
    size_t spotLightCount = 16;
    size_t pointLightCount = 0;
    for (size_t i = 0; i < spotLightCount; ++i) {
        auto spotLight = std::make_unique<SpotLight>();
        spotLight->SetName("StageLighting.SpotLight." + std::to_string(i));
        spotLight->SetEnabled(true);
        // ランダムに色を選択（3パターン）
        int colorIndex = GetRandomInt(0, 2);
        Vector4 chosenColor;
        if (colorIndex == 0) {
            chosenColor = Vector4{ 1.0f, 0.5f, 0.5f, 1.0f };
        } else if (colorIndex == 1) {
            chosenColor = Vector4{ 0.5f, 1.0f, 0.5f, 1.0f };
        } else {
            chosenColor = Vector4{ 0.5f, 0.5f, 1.0f, 1.0f };
        }
        spotLight->SetColor(chosenColor);
        spotLight->SetPosition(Vector3{
            static_cast<float>(i % 4) * 6.66f,
            10.0f,
            static_cast<float>(i / 4) * 6.66f
        });
        spotLight->SetDirection(Vector3{ 0.0f, -1.0f, 0.0f });
        spotLight->SetRange(16.0f);
        spotLight->SetInnerAngle(0.0f);
        spotLight->SetOuterAngle(0.3f);
        spotLight->SetIntensity(3.0f);
        spotLight->SetDecay(0.5f);
        spotLight->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        lightManager_->AddSpotLight(spotLight.get());
        spotLights_.push_back(spotLight.get());
        ctx->AddObject3D(std::move(spotLight));
    }
    for (size_t i = 0; i < pointLightCount; ++i) {
        auto pointLight = std::make_unique<PointLight>();
        pointLight->SetName("StageLighting.PointLight." + std::to_string(i));
        pointLight->SetEnabled(true);
        pointLight->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
        pointLight->SetPosition(Vector3{
            static_cast<float>(i % 4) * 6.66f,
            2.0f,
            static_cast<float>(i / 4) * 6.66f
        });
        pointLight->SetRange(10.0f);
        pointLight->SetIntensity(1.0f);
        pointLight->SetDecay(2.0f);
        pointLight->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        lightManager_->AddPointLight(pointLight.get());
        pointLights_.push_back(pointLight.get());
        ctx->AddObject3D(std::move(pointLight));
    }
}

void StageLighting::Finalize() {
}

void StageLighting::Update() {
    
}

} // namespace KashipanEngine
